param(
    [Parameter(Mandatory = $true)]
    [string] $ExecutablePath,
    [Parameter(Mandatory = $true)]
    [string] $RunAsUser,
    [string] $TaskName = 'HMS Encrypted PostgreSQL Backup'
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $ExecutablePath).Path
$passwordSecure = Read-Host "Password for scheduled task account $RunAsUser" -AsSecureString
$passwordPtr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($passwordSecure)
try {
    $password = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($passwordPtr)
    $action = New-ScheduledTaskAction -Execute $exe -Argument 'run-due' -WorkingDirectory (Split-Path -Parent $exe)
    $trigger = New-ScheduledTaskTrigger -Daily -At (Get-Date).AddMinutes(5)
    $trigger.Repetition.Interval = 'PT1H'
    $trigger.Repetition.Duration = 'P1D'
    $principal = New-ScheduledTaskPrincipal -UserId $RunAsUser -LogonType Password -RunLevel Limited
    $settings = New-ScheduledTaskSettingsSet `
        -StartWhenAvailable `
        -MultipleInstances IgnoreNew `
        -ExecutionTimeLimit (New-TimeSpan -Hours 12) `
        -RestartCount 3 `
        -RestartInterval (New-TimeSpan -Minutes 15)
    $task = New-ScheduledTask -Action $action -Trigger $trigger -Principal $principal -Settings $settings
    Register-ScheduledTask -TaskName $TaskName -InputObject $task -User $RunAsUser -Password $password -Force | Out-Null
    Write-Output "Registered '$TaskName' to run hourly as $RunAsUser. Configure protected environment variables for this account before starting it."
}
finally {
    if ($passwordPtr -ne [IntPtr]::Zero) {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($passwordPtr)
    }
    $password = $null
    $passwordSecure = $null
}
