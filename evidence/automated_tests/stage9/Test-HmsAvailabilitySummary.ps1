$ErrorActionPreference = 'Stop'
$repo = (Get-Location).Path
$helper = Join-Path $repo 'evidence\automated_tests\stage6_20261115\Get-HmsSevenDayAvailabilitySummary.ps1'
$tempCsv = Join-Path $env:TEMP ('hms-stage9-availability-fixture-' + [guid]::NewGuid().ToString('N') + '.csv')
$summaryPath = Join-Path $repo 'evidence\automated_tests\stage9\availability_calculation_test.json'
$startDate = [datetime]::ParseExact('2026-02-01','yyyy-MM-dd',[Globalization.CultureInfo]::InvariantCulture)
$writer = [IO.StreamWriter]::new($tempCsv,$false,[Text.UTF8Encoding]::new($false))
try {
    $writer.WriteLine('scheduled_local,scheduled_utc,observed_utc,success,http_status,elapsed_ms,error')
    $start = $startDate.AddHours(6)
    for ($day=0; $day -lt 7; $day++) {
        for ($minute=0; $minute -lt 960; $minute++) {
            if ($day -eq 6 -and $minute -ge 958) { continue }
            $slot = [DateTimeOffset]$start.AddDays($day).AddMinutes($minute)
            $local = $slot.ToString('yyyy-MM-dd HH:mm:ss zzz')
            $utc = $slot.ToUniversalTime().ToString('o')
            if ($day -eq 0 -and $minute -eq 0) {
                $writer.WriteLine(('"{0}","{1}","{1}",false,503,11,"synthetic outage"' -f $local,$utc))
            } else {
                $writer.WriteLine(('"{0}","{1}","{1}",true,200,9,""' -f $local,$utc))
            }
            if ($day -eq 0 -and $minute -eq 1) {
                $writer.WriteLine(('"{0}","{1}","{1}",true,200,9,"duplicate fixture row"' -f $local,$utc))
            }
        }
    }
    $outside = [DateTimeOffset]$startDate.AddHours(5).AddMinutes(59)
    $writer.WriteLine(('"{0}","{1}","{1}",true,200,1,"outside window"' -f $outside.ToString('yyyy-MM-dd HH:mm:ss zzz'),$outside.ToUniversalTime().ToString('o')))
} finally { $writer.Dispose() }

$summary = & $helper -InputCsv $tempCsv -StartDateLocal $startDate -OutputJson $summaryPath
$expected = @{
    expectedChecks=6720; uniqueObservedChecks=6718; successfulChecks=6717;
    failedChecks=3; missingChecks=2; duplicateRows=1; outOfWindowRows=1;
    availabilityPercent=99.9554
}
foreach ($name in $expected.Keys) {
    if ($summary[$name] -ne $expected[$name]) {
        throw "Availability calculation mismatch for $name`: expected $($expected[$name]), got $($summary[$name])."
    }
}
Remove-Item -LiteralPath $tempCsv -Force
$result = @(
    'Test=synthetic seven-day availability summary calculation'
    'ExpectedScheduledSlots=6720 (7 days x 960 minutes; [06:00,22:00) local)'
    'Fixture=one recorded HTTP 503; two absent slots; duplicate success row; one out-of-window success row'
    'ObservedUnique=6718; Successful=6717; FailedIncludingMissing=3'
    'AvailabilityPercent=99.9554'
    'Result=PASS'
) -join "`r`n"
[IO.File]::WriteAllText($summaryPath + '.test.txt',$result + "`r`n",[Text.UTF8Encoding]::new($false))
Write-Output $result
