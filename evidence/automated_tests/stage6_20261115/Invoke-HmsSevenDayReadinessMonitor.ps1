param(
    [Parameter(Mandatory=$true)][uri]$HealthUri,
    [Parameter(Mandatory=$true)][datetime]$StartDateLocal,
    [Parameter(Mandatory=$true)][string]$OutputCsv
)
$ErrorActionPreference = 'Stop'
if (-not $HealthUri.IsAbsoluteUri -or $HealthUri.Scheme -ne 'https' -or $HealthUri.UserInfo -or $HealthUri.Query -or $HealthUri.Fragment) {
    throw 'Use a trusted absolute HTTPS readiness URL without embedded credentials, query, or fragment.'
}
if ($StartDateLocal.TimeOfDay -ne [TimeSpan]::Zero) { throw 'StartDateLocal must be midnight local time.' }
$start = $StartDateLocal.Date.AddHours(6)
$end = $StartDateLocal.Date.AddDays(6).AddHours(22)
$parent = Split-Path -Parent $OutputCsv
if ($parent -and -not (Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Path $parent -ErrorAction Stop | Out-Null }
if (-not (Test-Path -LiteralPath $OutputCsv)) {
    [IO.File]::WriteAllText($OutputCsv,
        "scheduled_local,scheduled_utc,observed_utc,success,http_status,elapsed_ms,error`r`n",
        [Text.UTF8Encoding]::new($false))
}
$existing = @{}
foreach ($row in (Import-Csv -LiteralPath $OutputCsv)) {
    if ($row.scheduled_local) { $existing[$row.scheduled_local] = $true }
}
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Csv-Field([string]$value) { '"' + ($value -replace '"','""') + '"' }
function Record-Probe([datetime]$scheduled, [bool]$wasRunning) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $ok = $false; $status = ''; $failure = ''; $observed = [DateTimeOffset]::UtcNow
    if (-not $wasRunning) {
        $failure = 'monitor was not running at the scheduled check time'
    } else {
        try {
            $response = Invoke-WebRequest -UseBasicParsing -Uri $HealthUri -Method Get -TimeoutSec 10
            $status = [string][int]$response.StatusCode
            if ([int]$response.StatusCode -eq 200) {
                $body = $response.Content | ConvertFrom-Json
                $ok = ($body.status -eq 'ready' -and $body.checks.postgres -eq 'ok')
                if (-not $ok) { $failure = 'readiness response did not report ready and postgres ok' }
            } else { $failure = 'readiness endpoint did not return HTTP 200' }
        } catch {
            if ($_.Exception.Response) { $status = [string][int]$_.Exception.Response.StatusCode }
            $failure = 'readiness request or response validation failed'
        }
    }
    $watch.Stop()
    $localText = ([DateTimeOffset]$scheduled).ToString('yyyy-MM-dd HH:mm:ss zzz')
    $row = @(
        (Csv-Field $localText),
        (Csv-Field ([DateTimeOffset]$scheduled).ToUniversalTime().ToString('o')),
        (Csv-Field $observed.ToString('o')),
        ([string]$ok).ToLowerInvariant(),$status,[string]$watch.ElapsedMilliseconds,
        (Csv-Field $failure)
    ) -join ','
    [IO.File]::AppendAllText($OutputCsv,$row + "`r`n",[Text.UTF8Encoding]::new($false))
    $existing[$localText] = $true
}

for ($day=0; $day -lt 7; $day++) {
    for ($minute=0; $minute -lt 960; $minute++) {
        $scheduled = $start.AddDays($day).AddMinutes($minute)
        $localText = ([DateTimeOffset]$scheduled).ToString('yyyy-MM-dd HH:mm:ss zzz')
        if ($existing.ContainsKey($localText)) { continue }
        while ((Get-Date) -lt $scheduled) {
            $remaining = $scheduled.Subtract((Get-Date)).TotalMilliseconds
            Start-Sleep -Milliseconds ([Math]::Min(1000,[Math]::Max(1,[int]$remaining)))
        }
        $lateBy = (Get-Date).Subtract($scheduled).TotalSeconds
        Record-Probe $scheduled ($lateBy -le 15)
    }
}

$summaryScript = Join-Path $PSScriptRoot 'Get-HmsSevenDayAvailabilitySummary.ps1'
if (-not (Test-Path -LiteralPath $summaryScript -PathType Leaf)) {
    throw 'Availability summary helper was not found.'
}
$summaryPath = $OutputCsv + '.summary.json'
& $summaryScript -InputCsv $OutputCsv -StartDateLocal $StartDateLocal -OutputJson $summaryPath | Out-Null
Write-Output "Seven-day availability summary: $summaryPath"
