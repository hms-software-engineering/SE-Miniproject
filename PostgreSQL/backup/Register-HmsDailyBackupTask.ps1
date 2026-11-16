param(
    [Parameter(Mandatory=$true)][string]$BackupScriptPath,
    [Parameter(Mandatory=$true)][string]$PgBinPath,
    [Parameter(Mandatory=$true)][string]$DbHost,
    [Parameter(Mandatory=$true)][ValidateRange(1,65535)][int]$DbPort,
    [Parameter(Mandatory=$true)][string]$Database,
    [Parameter(Mandatory=$true)][string]$Username,
    [Parameter(Mandatory=$true)][string]$BackupDirectory,
    [Parameter(Mandatory=$true)][string]$DatabaseDataDirectory,
    [string]$PgPassFile,
    [string]$TaskName = 'HMS-Daily-Encrypted-PostgreSQL-Backup',
    [datetime]$DailyAt = [datetime]::Today.AddHours(2)
)
$ErrorActionPreference = 'Stop'
foreach ($value in @($DbHost,$Database,$Username)) {
    if ($value -match '[\s"]') { throw 'Database host, name, and username must not contain whitespace or quotes.' }
}
if (-not (Test-Path -LiteralPath $BackupScriptPath -PathType Leaf)) { throw 'Backup script was not found.' }
if (-not (Test-Path -LiteralPath $PgBinPath -PathType Container)) { throw 'PostgreSQL client tools were not found.' }
if (-not (Test-Path -LiteralPath $DatabaseDataDirectory -PathType Container)) { throw 'Use the configured live database data directory; it must exist.' }
$backupFull = [IO.Path]::GetFullPath($BackupDirectory).TrimEnd('\') + '\'
$dataFull = [IO.Path]::GetFullPath($DatabaseDataDirectory).TrimEnd('\') + '\'
if ($backupFull.StartsWith($dataFull,[StringComparison]::OrdinalIgnoreCase) -or
    $dataFull.StartsWith($backupFull,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Backups must use a separate directory from the live PostgreSQL data directory.'
}
if ($PgPassFile -and -not (Test-Path -LiteralPath $PgPassFile -PathType Leaf)) {
    throw 'PGPASSFILE must exist and be provisioned outside the repository.'
}
if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) {
    throw "Task '$TaskName' already exists; inspect it manually before changing it."
}
if (-not (Test-Path -LiteralPath $BackupDirectory)) {
    New-Item -ItemType Directory -Path $BackupDirectory -ErrorAction Stop | Out-Null
}
$powerShell = Join-Path $PSHOME 'powershell.exe'
$taskArguments = @(
    '-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass',
    '-File',([IO.Path]::GetFullPath($BackupScriptPath)),
    '-Mode','Backup','-PgBinPath',([IO.Path]::GetFullPath($PgBinPath)),
    '-DbHost',$DbHost,'-DbPort',[string]$DbPort,'-Database',$Database,
    '-Username',$Username,'-BackupDirectory',$backupFull,
    '-DatabaseDataDirectory',$dataFull
)
if ($PgPassFile) { $taskArguments += @('-PgPassFile',[IO.Path]::GetFullPath($PgPassFile)) }
$quote = { param([string]$v) '"' + ($v -replace '"','\"') + '"' }
$argumentLine = ($taskArguments | ForEach-Object { & $quote $_ }) -join ' '
$action = New-ScheduledTaskAction -Execute $powerShell -Argument $argumentLine `
    -WorkingDirectory (Split-Path -Parent ([IO.Path]::GetFullPath($BackupScriptPath)))
$trigger = New-ScheduledTaskTrigger -Daily -At $DailyAt
$settings = New-ScheduledTaskSettingsSet -StartWhenAvailable `
    -ExecutionTimeLimit (New-TimeSpan -Hours 4) -MultipleInstances IgnoreNew `
    -RestartCount 2 -RestartInterval (New-TimeSpan -Minutes 15)
$currentUser = [Security.Principal.WindowsIdentity]::GetCurrent().Name
$principal = New-ScheduledTaskPrincipal -UserId $currentUser `
    -LogonType Interactive -RunLevel Limited
$task = New-ScheduledTask -Action $action -Trigger $trigger `
    -Settings $settings -Principal $principal `
    -Description 'Runs the HMS encrypted PostgreSQL backup as the current user; DPAPI keys are user-scoped.'
Register-ScheduledTask -TaskName $TaskName -InputObject $task | Out-Null
Write-Output "Registered daily backup task '$TaskName' for $currentUser at $($DailyAt.ToString('HH:mm'))."
Write-Output 'The task runs only while that account is logged on. Confirm its first run and restore a separate disposable database before marking TC-REL-01 PASS.'
