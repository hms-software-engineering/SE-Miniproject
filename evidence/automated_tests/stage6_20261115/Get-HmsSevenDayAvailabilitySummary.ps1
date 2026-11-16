param(
    [Parameter(Mandatory=$true)][string]$InputCsv,
    [Parameter(Mandatory=$true)][datetime]$StartDateLocal,
    [Parameter(Mandatory=$true)][string]$OutputJson
)
$ErrorActionPreference = 'Stop'
if ($StartDateLocal.TimeOfDay -ne [TimeSpan]::Zero) {
    throw 'StartDateLocal must be midnight local time.'
}
if (-not (Test-Path -LiteralPath $InputCsv -PathType Leaf)) {
    throw 'Monitoring CSV was not found.'
}

$expected = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$observed = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$successful = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$start = $StartDateLocal.Date.AddHours(6)
$endExclusive = $StartDateLocal.Date.AddDays(6).AddHours(22)
for ($day = 0; $day -lt 7; $day++) {
    for ($minute = 0; $minute -lt 960; $minute++) {
        $slot = [DateTimeOffset]$start.AddDays($day).AddMinutes($minute)
        [void]$expected.Add($slot.ToString('yyyy-MM-dd HH:mm:ss zzz'))
    }
}

$outOfWindowRows = 0
$rows = @(Import-Csv -LiteralPath $InputCsv)
foreach ($row in $rows) {
    $key = [string]$row.scheduled_local
    if (-not $expected.Contains($key)) {
        $outOfWindowRows++
        continue
    }
    [void]$observed.Add($key)
    if ([string]$row.success -ieq 'true' -and [string]$row.http_status -eq '200') {
        [void]$successful.Add($key)
    }
}

$expectedCount = $expected.Count
$successCount = $successful.Count
$observedCount = $observed.Count
$missingCount = $expectedCount - $observedCount
$summary = [ordered]@{
    startDateLocal = $StartDateLocal.ToString('yyyy-MM-dd')
    windowStartLocal = $start.ToString('yyyy-MM-dd HH:mm:ss zzz')
    windowEndExclusiveLocal = ([DateTimeOffset]$endExclusive).ToString('yyyy-MM-dd HH:mm:ss zzz')
    dailyWindow = '[06:00, 22:00) local; one scheduled slot per minute'
    expectedChecks = $expectedCount
    uniqueObservedChecks = $observedCount
    successfulChecks = $successCount
    failedChecks = $expectedCount - $successCount
    missingChecks = $missingCount
    duplicateRows = $rows.Count - $observedCount - $outOfWindowRows
    outOfWindowRows = $outOfWindowRows
    availabilityPercent = [math]::Round(100.0 * $successCount / $expectedCount, 4)
}
$json = $summary | ConvertTo-Json -Depth 4
$outputParent = Split-Path -Parent $OutputJson
if ($outputParent -and -not (Test-Path -LiteralPath $outputParent)) {
    New-Item -ItemType Directory -Path $outputParent -Force -ErrorAction Stop | Out-Null
}
[IO.File]::WriteAllText($OutputJson, $json + "`r`n", [Text.UTF8Encoding]::new($false))
$summary
