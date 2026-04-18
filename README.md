# SharePoint Sync Speedup

A lightweight, silent Win32 utility that eliminates the ~8-hour delay for OneDrive SharePoint library auto-mount at user logon. Designed for deployment via Microsoft Intune as a Win32 app in user context.

## Problem

When OneDrive is configured to sync SharePoint libraries, it can take up to 8 hours after logon before those libraries appear in File Explorer. This is caused by the `TimerAutoMount` registry value under `HKCU:\Software\Microsoft\OneDrive\Accounts`. Setting it to `1` triggers an immediate sync.

## Solution

This utility:
1. Deploys a PowerShell script to `%LOCALAPPDATA%\SharePointSyncSpeedup\`
2. Creates a user-context scheduled task to run the script at every logon
3. Runs the script once immediately at install time

The binary is a **Windows GUI subsystem app** — no console window or UI is ever shown to the user.

## Build

### Prerequisites
- CMake 3.16+
- MSVC (Visual Studio 2019+ Build Tools) or MinGW-w64

### Build Steps

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The output binary is `build\Release\SharePointSyncSpeedup.exe` (MSVC) or `build\SharePointSyncSpeedup.exe` (MinGW).

## Usage

```powershell
# Install (creates script + scheduled task + runs immediately)
SharePointSyncSpeedup.exe

# Uninstall (removes scheduled task + script directory)
SharePointSyncSpeedup.exe /uninstall
```

## Intune Deployment

See [docs/INTUNE_DEPLOYMENT.md](docs/INTUNE_DEPLOYMENT.md) for complete packaging and deployment instructions.

**Quick reference:**

| Setting | Value |
|---------|-------|
| Install command | `SharePointSyncSpeedup.exe` |
| Uninstall command | `SharePointSyncSpeedup.exe /uninstall` |
| Install behavior | User |
| Detection rule | File exists: `%LOCALAPPDATA%\SharePointSyncSpeedup\sharepointspeedup.ps1` |

## Security

- Runs entirely in user context — no admin privileges required
- No network access — purely local registry operations
- PowerShell script is embedded in the binary at compile time
- All child processes launched with `CREATE_NO_WINDOW`
- Script stored in user-only writable `%LOCALAPPDATA%`

See [SECURITY.md](SECURITY.md) for the full security policy.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for branch strategy, coding standards, and workflow.

## License

[MIT](LICENSE)
