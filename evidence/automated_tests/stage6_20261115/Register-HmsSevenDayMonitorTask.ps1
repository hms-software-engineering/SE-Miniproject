param(
    [Parameter(Mandatory=$true)][uri]$HealthUri,
    [Parameter(Mandatory=$true)][datetime]$StartDateLocal,
    [Parameter(Mandatory=$true)][string]$MonitorScriptPath,
    [Parameter(Mandatory=$true)][string]$OutputCsv,
    [string]$TaskName = 'HMS-Seven-Day-Readiness-Monitor'
)
$ErrorActionPreference = 'Stop'
if (-not $HealthUri.IsAbsoluteUri -or $HealthUri.Scheme -ne 'https' -or $HealthUri.UserInfo -or $HealthUri.Query -or $HealthUri.Fragment) {
    throw 'Provide a trusted HTTPS readiness URL without credentials or query parameters.'
}
if ($StartDateLocal.TimeOfDay -ne [TimeSpan]::Zero) { throw 'StartDateLocal must be midnight local time.' }
if (-not (Test-Path -LiteralPath $MonitorScriptPath -PathType Leaf)) { throw 'Monitor script was not found.' }
$launchAt = $StartDateLocal.Date.AddHours(6)
if ($launchAt -le (Get-Date)) { throw 'Choose a future 06:00 local start so all seven days are observed.' }
if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) {
    throw "Task '$TaskName' already exists; inspect it manually before changing it."
}
$powerShell = Join-Path $PSHOME 'powershell.exe'
$escapedScript = [IO.Path]::GetFullPath($MonitorScriptPath).Replace('"','\"')
$escapedOutput = [IO.Path]::GetFullPath($OutputCsv).Replace('"','\"')
$escapedUri = $HealthUri.AbsoluteUri.Replace('"','\"')
$arguments = '-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "{0}" -HealthUri "{1}" -StartDateLocal "{2}" -OutputCsv "{3}"' -f `
    $escapedScript,$escapedUri,$StartDateLocal.ToString('yyyy-MM-dd'),$escapedOutput
$action = New-ScheduledTaskAction -Execute $powerShell -Argument $arguments `
    -WorkingDirectory (Split-Path -Parent ([IO.Path]::GetFullPath($MonitorScriptPath)))
$triggers = @(
    (New-ScheduledTaskTrigger -Once -At $launchAt),
    (New-ScheduledTaskTrigger -AtStartup)
)
$settings = New-ScheduledTaskSettingsSet -StartWhenAvailable `
    -ExecutionTimeLimit (New-TimeSpan -Days 8) -MultipleInstances IgnoreNew `
    -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 2)
$principal = New-ScheduledTaskPrincipal `
    -UserId ([Security.Principal.WindowsIdentity]::GetCurrent().Name) `
    -LogonType Interactive -RunLevel Limited
$task = New-ScheduledTask -Action $action -Trigger $triggers `
    -Settings $settings -Principal $principal `
    -Description 'Samples HMS PostgreSQL readiness each service minute for seven days; records are append-only.'
Register-ScheduledTask -TaskName $TaskName -InputObject $task | Out-Null
Write-Output "Registered '$TaskName'; it begins $($launchAt.ToString('yyyy-MM-dd HH:mm zzz'))."
Write-Output 'The task resumes after process/host restart when the configured user signs in. Verify the Task Scheduler history and the complete CSV after seven days.'
