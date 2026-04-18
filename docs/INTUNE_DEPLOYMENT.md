# Intune Deployment Guide

## Overview

This utility is packaged as a Win32 app (`.intunewin`) and deployed via Microsoft Intune in **user context**. It runs silently — no windows or prompts are shown to the user.

## Prerequisites

- [Microsoft Win32 Content Prep Tool](https://github.com/microsoft/Microsoft-Win32-Content-Prep-Tool) (`IntuneWinAppUtil.exe`)
- A built `SharePointSyncSpeedup.exe` (see [README.md](../README.md) for build instructions)

## Packaging

1. Place `SharePointSyncSpeedup.exe` in a staging folder (e.g., `.\package\`).
2. Run the content prep tool:
   ```powershell
   IntuneWinAppUtil.exe `
       -c .\package `
       -s SharePointSyncSpeedup.exe `
       -o .\output
   ```
3. This produces `SharePointSyncSpeedup.intunewin` in `.\output\`.

## Intune App Configuration

In the Microsoft Intune admin center, create a new **Windows app (Win32)**:

### App Information
| Field | Value |
|-------|-------|
| Name | SharePoint Sync Speedup |
| Description | Eliminates the 8-hour delay for OneDrive SharePoint library auto-mount at logon |
| Publisher | Your Organization |

### Program
| Field | Value |
|-------|-------|
| Install command | `SharePointSyncSpeedup.exe /quiet` |
| Uninstall command | `SharePointSyncSpeedup.exe /uninstall` |
| Install behavior | **User** |
| Device restart behavior | No specific action |

### Requirements
| Field | Value |
|-------|-------|
| OS architecture | 64-bit |
| Minimum OS version | Windows 10 1809 |

### Detection Rules

Use a **manual detection rule**:

| Type | Path | File/Folder | Detection method |
|------|------|-------------|------------------|
| File | `%LOCALAPPDATA%\SharePointSyncSpeedup` | `sharepointspeedup.ps1` | File or folder exists |

### Assignments

Assign to **All Users** or a specific user group. The app installs per-user on any Intune-managed device (corporate or personal).

## What It Does

1. Creates `%LOCALAPPDATA%\SharePointSyncSpeedup\sharepointspeedup.ps1`
2. Registers a scheduled task (`SharePointSyncSpeedup`) to run the script at each user logon
3. Runs the script once immediately to apply the fix right away

## Uninstall Behavior

The uninstall command:
1. Deletes the `SharePointSyncSpeedup` scheduled task
2. Removes the `%LOCALAPPDATA%\SharePointSyncSpeedup\` directory and all contents

## Troubleshooting

| Symptom | Check |
|---------|-------|
| Detection fails | Verify `%LOCALAPPDATA%\SharePointSyncSpeedup\sharepointspeedup.ps1` exists |
| Task not created | Run `schtasks /Query /TN SharePointSyncSpeedup` in user context |
| Script not running | Check Task Scheduler history for the `SharePointSyncSpeedup` task |
| Intune install fails | Check Intune Management Extension logs: `%ProgramData%\Microsoft\IntuneManagementExtension\Logs\` |
