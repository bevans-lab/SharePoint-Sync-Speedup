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
static const wchar_t* const TASK_NAME      = L"SharePointSyncSpeedup";

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

    // 3. Create scheduled task (at logon, user context)
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

    // 4. Run the script once immediately for instant effect
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

    // 2. Remove app directory
    fs::path appDir = GetAppDir();
    if (!appDir.empty())
    {
        std::error_code ec;
        fs::remove_all(appDir, ec);
    }

    return 0;
}

// ── Entry point (Win32 GUI subsystem) ───────────────────────────────────────
int WINAPI wWinMain(
    _In_ HINSTANCE     /*hInstance*/,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_ LPWSTR        lpCmdLine,
    _In_ int           /*nShowCmd*/)
{
    std::wstring args(lpCmdLine);

    // Case-insensitive check for /uninstall
    if (args.find(L"/uninstall") != std::wstring::npos ||
        args.find(L"/UNINSTALL") != std::wstring::npos ||
        args.find(L"/Uninstall") != std::wstring::npos)
    {
        return Uninstall();
    }

    return Install();
}
