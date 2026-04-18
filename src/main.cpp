/*
 * SharePoint Sync Speedup
 *
 * Silent Win32 utility that:
 *   1. Deploys a PowerShell script to %LOCALAPPDATA%
 *   2. Creates a user-context logon scheduled task to run it
 *   3. Runs the script once immediately
 *
 * Supports /uninstall to cleanly remove all artifacts.
 * Designed for Intune Win32 app deployment in user context.
 *
 * SPDX-License-Identifier: MIT
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// ── Embedded PowerShell script ──────────────────────────────────────────────
static const char* const PS1_CONTENT = R"PS1(
# Speed up OneDrive auto-mount of configured SharePoint libraries
# Runs in user context

$basePath = "HKCU:\Software\Microsoft\OneDrive\Accounts"
$accounts = Get-ChildItem -Path $basePath -ErrorAction SilentlyContinue

foreach ($acct in $accounts) {
    New-ItemProperty `
        -Path $acct.PSPath `
        -Name "TimerAutoMount" `
        -PropertyType QWord `
        -Value 1 `
        -Force | Out-Null
}

exit 0
)PS1";

static const wchar_t* const APP_DIR_NAME   = L"SharePointSyncSpeedup";
static const wchar_t* const SCRIPT_NAME    = L"sharepointspeedup.ps1";
static const wchar_t* const EXE_NAME       = L"SharePointSyncSpeedup.exe";
static const wchar_t* const TASK_NAME      = L"SharePointSyncSpeedup";
static const wchar_t* const ARP_KEY        = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SharePointSyncSpeedup";
static const wchar_t* const DISPLAY_NAME   = L"SharePoint Sync Speedup";
static const wchar_t* const PUBLISHER      = L"bevans-lab";
static const wchar_t* const APP_VERSION    = L"1.0.0";

// ── Helpers ─────────────────────────────────────────────────────────────────

// Get %LOCALAPPDATA%\SharePointSyncSpeedup
static fs::path GetAppDir()
{
    wchar_t* localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)))
        return {};

    fs::path dir = fs::path(localAppData) / APP_DIR_NAME;
    CoTaskMemFree(localAppData);
    return dir;
}

// Run a process completely hidden (no window), wait for exit, return exit code.
static DWORD RunHidden(const std::wstring& cmdLine)
{
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};

    // CreateProcessW needs a mutable command-line buffer
    std::wstring cmd = cmdLine;

    if (!CreateProcessW(
            nullptr,
            cmd.data(),
            nullptr, nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr, nullptr,
            &si, &pi))
    {
        return 1;
    }

    WaitForSingleObject(pi.hProcess, 30000); // 30 s timeout

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode;
}

// ── Install ─────────────────────────────────────────────────────────────────
static int Install()
{
    // 1. Create app directory
    fs::path appDir = GetAppDir();
    if (appDir.empty())
        return 1;

    std::error_code ec;
    fs::create_directories(appDir, ec);
    if (ec)
        return 1;

    // 2. Write the embedded PS1 script
    fs::path scriptPath = appDir / SCRIPT_NAME;
    HANDLE hFile = CreateFileW(
        scriptPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE)
        return 1;

    DWORD written = 0;
    DWORD len = static_cast<DWORD>(strlen(PS1_CONTENT));
    BOOL ok = WriteFile(hFile, PS1_CONTENT, len, &written, nullptr);
    CloseHandle(hFile);

    if (!ok || written != len)
        return 1;

    // 3. Copy the running exe into the app directory for uninstall support
    fs::path installedExe = appDir / EXE_NAME;
    {
        wchar_t selfPath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
        std::error_code cpEc;
        fs::copy_file(selfPath, installedExe,
                      fs::copy_options::overwrite_existing, cpEc);
        // Non-fatal: Intune manages uninstall itself, so the exe copy
        // is only needed for local Apps & Features uninstall.
    }

    // 4. Write Add/Remove Programs (ARP) registry entry
    {
        HKEY hKey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, ARP_KEY, 0, nullptr,
                            0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
        {
            std::wstring uninstallCmd = L"\"" + installedExe.wstring() + L"\" /uninstall";

            RegSetValueExW(hKey, L"DisplayName",     0, REG_SZ, reinterpret_cast<const BYTE*>(DISPLAY_NAME),   static_cast<DWORD>((wcslen(DISPLAY_NAME)   + 1) * sizeof(wchar_t)));
            RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, reinterpret_cast<const BYTE*>(uninstallCmd.c_str()), static_cast<DWORD>((uninstallCmd.size() + 1) * sizeof(wchar_t)));
            RegSetValueExW(hKey, L"InstallLocation", 0, REG_SZ, reinterpret_cast<const BYTE*>(appDir.c_str()),  static_cast<DWORD>((appDir.wstring().size() + 1) * sizeof(wchar_t)));
            RegSetValueExW(hKey, L"Publisher",       0, REG_SZ, reinterpret_cast<const BYTE*>(PUBLISHER),       static_cast<DWORD>((wcslen(PUBLISHER)       + 1) * sizeof(wchar_t)));
            RegSetValueExW(hKey, L"DisplayVersion",  0, REG_SZ, reinterpret_cast<const BYTE*>(APP_VERSION),     static_cast<DWORD>((wcslen(APP_VERSION)     + 1) * sizeof(wchar_t)));
            DWORD noModify = 1;
            RegSetValueExW(hKey, L"NoModify",  0, REG_DWORD, reinterpret_cast<const BYTE*>(&noModify), sizeof(DWORD));
            RegSetValueExW(hKey, L"NoRepair",  0, REG_DWORD, reinterpret_cast<const BYTE*>(&noModify), sizeof(DWORD));

            RegCloseKey(hKey);
        }
    }

    // 5. Create scheduled task (at logon, user context)
    //    Uses Register-ScheduledTask — works in user context without elevation.
    //    The -AtLogOn trigger fires for the current user at every interactive logon.
    std::wstring registerTask =
        L"powershell.exe -WindowStyle Hidden -NonInteractive -ExecutionPolicy Bypass -Command \""
        L"$action = New-ScheduledTaskAction"
            L" -Execute 'powershell.exe'"
            L" -Argument '-WindowStyle Hidden -NonInteractive -ExecutionPolicy Bypass"
                L" -File \\\"" + scriptPath.wstring() + L"\\\"';"
        L" $trigger = New-ScheduledTaskTrigger -AtLogOn;"
        L" $settings = New-ScheduledTaskSettingsSet"
            L" -Hidden"
            L" -AllowStartIfOnBatteries"
            L" -DontStopIfGoingOnBatteries"
            L" -ExecutionTimeLimit (New-TimeSpan -Minutes 5);"
        L" Register-ScheduledTask"
            L" -TaskName '" + std::wstring(TASK_NAME) + L"'"
            L" -Action $action"
            L" -Trigger $trigger"
            L" -Settings $settings"
            L" -Force"
        L"\"";

    DWORD taskResult = RunHidden(registerTask);
    if (taskResult != 0)
        return 1;

    // 6. Run the script once immediately for instant effect
    std::wstring runNow =
        L"powershell.exe -WindowStyle Hidden -NonInteractive"
        L" -ExecutionPolicy Bypass"
        L" -File \"" + scriptPath.wstring() + L"\"";

    RunHidden(runNow);

    return 0;
}

// ── Uninstall ───────────────────────────────────────────────────────────────
static int Uninstall()
{
    // 1. Delete the scheduled task (ignore errors if already gone)
    std::wstring unregisterTask =
        L"powershell.exe -WindowStyle Hidden -NonInteractive -ExecutionPolicy Bypass -Command \""
        L"Unregister-ScheduledTask -TaskName '" + std::wstring(TASK_NAME) + L"'"
        L" -Confirm:$false -ErrorAction SilentlyContinue"
        L"\"";
    RunHidden(unregisterTask);

    // 2. Remove ARP registry entry
    RegDeleteKeyW(HKEY_CURRENT_USER, ARP_KEY);

    // 3. Remove app directory (script + exe copy)
    //    The running exe may be inside this directory, so schedule deletion
    //    of the exe via MoveFileEx if direct removal fails.
    fs::path appDir = GetAppDir();
    if (!appDir.empty())
    {
        // Try to delete everything except our own exe first
        std::error_code ec;
        fs::path selfExe = appDir / EXE_NAME;
        for (auto& entry : fs::directory_iterator(appDir, ec))
        {
            if (entry.path() != selfExe)
                fs::remove(entry.path(), ec);
        }

        // Try removing the exe and directory; if locked, schedule for reboot
        if (!fs::remove(selfExe, ec))
            MoveFileExW(selfExe.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);

        if (!fs::remove(appDir, ec))
            MoveFileExW(appDir.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    }

    return 0;
}

// ── Case-insensitive substring search ────────────────────────────────────────
static bool ContainsCI(const std::wstring& haystack, const wchar_t* needle)
{
    std::wstring h = haystack, n = needle;
    CharLowerBuffW(h.data(), static_cast<DWORD>(h.size()));
    CharLowerBuffW(n.data(), static_cast<DWORD>(n.size()));
    return h.find(n) != std::wstring::npos;
}

// ── Entry point (Win32 GUI subsystem) ───────────────────────────────────────
int WINAPI wWinMain(
    _In_ HINSTANCE     /*hInstance*/,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_ LPWSTR        lpCmdLine,
    _In_ int           /*nShowCmd*/)
{
    std::wstring args(lpCmdLine);

    bool quiet = ContainsCI(args, L"/quiet") || ContainsCI(args, L"/silent");

    // Uninstall — always silent
    if (ContainsCI(args, L"/uninstall"))
        return Uninstall();

    // Interactive install: confirm before proceeding
    if (!quiet)
    {
        int choice = MessageBoxW(
            nullptr,
            L"SharePoint Sync Speedup\n\n"
            L"This will install a logon script that eliminates the ~8-hour\n"
            L"delay for OneDrive SharePoint library sync.\n\n"
            L"A scheduled task will run automatically at every login.\n\n"
            L"Install now?",
            L"SharePoint Sync Speedup — Install",
            MB_OKCANCEL | MB_ICONINFORMATION);

        if (choice != IDOK)
            return 1;  // User cancelled
    }

    int result = Install();

    if (!quiet)
    {
        if (result == 0)
        {
            MessageBoxW(
                nullptr,
                L"Installation complete.\n\n"
                L"The SharePoint sync speedup script has been installed and\n"
                L"will run automatically at every logon.\n\n"
                L"You can uninstall from Settings > Apps > Apps & features.",
                L"SharePoint Sync Speedup",
                MB_OK | MB_ICONINFORMATION);
        }
        else
        {
            MessageBoxW(
                nullptr,
                L"Installation failed.\n\n"
                L"The installer was unable to complete. Please try running\n"
                L"the installer again or contact your IT administrator.",
                L"SharePoint Sync Speedup — Error",
                MB_OK | MB_ICONERROR);
        }
    }

    return result;
}
