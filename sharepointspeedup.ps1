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