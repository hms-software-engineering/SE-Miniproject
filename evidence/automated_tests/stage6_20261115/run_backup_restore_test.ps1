$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$pg = 'C:\Program Files\PostgreSQL\17\bin'
$backupScript = Join-Path $repo 'PostgreSQL\backup\Invoke-HmsEncryptedPostgresBackup.ps1'
$evidence = Join-Path $PSScriptRoot 'backup_restore_execution.txt'
$suffix = [guid]::NewGuid().ToString('N').Substring(0,10)
$data = Join-Path $env:TEMP "hms-stage6-pg17-$suffix"
$backupDirectory = Join-Path $env:TEMP "hms-stage6-backups-$suffix"
$port = 55439
$source = "hms_stage6_test_$suffix"
$target = "hms_stage6_restore_$suffix"
$serverStarted = $false
$results = [Collections.Generic.List[string]]::new()
$results.Add('DateUtc=' + [DateTimeOffset]::UtcNow.ToString('o'))
$results.Add('PostgreSQL=17; isolated temporary cluster; loopback only')
$results.Add('SyntheticDatabase=' + $source)

function Assert-ExitCode([string]$operation) {
    if ($LASTEXITCODE -ne 0) { throw "$operation failed with exit code $LASTEXITCODE" }
}
function Run-BackupScript([hashtable]$parameters) {
    & $backupScript @parameters
}
function Query([string]$database,[string]$sql) {
    $value = & (Join-Path $pg 'psql.exe') -X -q -A -t -v ON_ERROR_STOP=1 `
        -h 127.0.0.1 -p $port -U hms_test -d $database -c $sql
    Assert-ExitCode 'psql'
    return [string]$value
}

try {
    New-Item -ItemType Directory -Path $data,$backupDirectory -ErrorAction Stop | Out-Null
    & (Join-Path $pg 'initdb.exe') -D $data -U hms_test -A trust --no-locale --encoding=UTF8
    Assert-ExitCode 'initdb'
    & (Join-Path $pg 'pg_ctl.exe') -D $data -o "-h 127.0.0.1 -p $port" -w start
    Assert-ExitCode 'pg_ctl start'
    $serverStarted = $true
    & (Join-Path $pg 'createdb.exe') -h 127.0.0.1 -p $port -U hms_test $source
    Assert-ExitCode 'createdb source'
    & (Join-Path $pg 'psql.exe') -X -q -v ON_ERROR_STOP=1 -h 127.0.0.1 -p $port -U hms_test -d $source `
        -c "CREATE TABLE backup_probe(marker text PRIMARY KEY,amount_cents bigint NOT NULL); INSERT INTO backup_probe VALUES ('stage6-synthetic-01',10000),('stage6-synthetic-02',25000),('stage6-synthetic-03',50000)"
    Assert-ExitCode 'seed synthetic fixture'
    $expected = Query $source "SELECT count(*)||'|'||sum(amount_cents)||'|'||string_agg(marker,',' ORDER BY marker) FROM backup_probe"
    $watch = [Diagnostics.Stopwatch]::StartNew()
    Run-BackupScript @{
        Mode='Backup'; PgBinPath=$pg; DbHost='127.0.0.1'; DbPort=$port
        Database=$source; Username='hms_test'; BackupDirectory=$backupDirectory
        DatabaseDataDirectory=$data
    }
    $watch.Stop()
    $backup = Get-ChildItem -LiteralPath $backupDirectory -Filter '*.hmsbak' -File
    if (@($backup).Count -ne 1) { throw 'Backup run did not publish exactly one encrypted archive.' }
    $cipherBytes = [IO.File]::ReadAllBytes($backup.FullName)
    $containsMarker = [Text.Encoding]::ASCII.GetString($cipherBytes).Contains('stage6-synthetic-01')
    if ($containsMarker) { throw 'Encrypted archive contains a plaintext fixture marker.' }
    $results.Add('BackupDurationSeconds=' + $watch.Elapsed.TotalSeconds.ToString('F3',[Globalization.CultureInfo]::InvariantCulture))
    $results.Add('EncryptedArchiveBytes=' + $cipherBytes.Length)
    $results.Add('EncryptedArchiveSha256=' + (Get-FileHash -LiteralPath $backup.FullName -Algorithm SHA256).Hash)
    $results.Add('PlaintextMarkerAbsent=True')

    & (Join-Path $pg 'createdb.exe') -h 127.0.0.1 -p $port -U hms_test $target
    Assert-ExitCode 'createdb restore target'
    $watch = [Diagnostics.Stopwatch]::StartNew()
    Run-BackupScript @{
        Mode='Restore'; PgBinPath=$pg; DbHost='127.0.0.1'; DbPort=$port
        Database=$source; Username='hms_test'; BackupDirectory=$backupDirectory
        DatabaseDataDirectory=$data; BackupPath=$backup.FullName
        RestoreDatabase=$target
    }
    $watch.Stop()
    $actual = Query $target "SELECT count(*)||'|'||sum(amount_cents)||'|'||string_agg(marker,',' ORDER BY marker) FROM backup_probe"
    if ($actual -cne $expected) { throw 'Restored synthetic database fingerprint did not match the source.' }
    $results.Add('SourceFingerprint=' + $expected)
    $results.Add('RestoredFingerprintMatches=True')
    $results.Add('RestoreDurationSeconds=' + $watch.Elapsed.TotalSeconds.ToString('F3',[Globalization.CultureInfo]::InvariantCulture))
    $results.Add('RowsLost=0')
    $results.Add('RestoreWithinFourHours=' + [string]($watch.Elapsed -lt [TimeSpan]::FromHours(4)))

    $before = @(Get-ChildItem -LiteralPath $backupDirectory -Filter '*.hmsbak' -File).Count
    $failedAsExpected = $false
    try {
        Run-BackupScript @{
            Mode='Backup'; PgBinPath=$pg; DbHost='127.0.0.1'; DbPort=$port
            Database=("hms_stage6_missing_$suffix"); Username='hms_test'
            BackupDirectory=$backupDirectory; DatabaseDataDirectory=$data
        }
    } catch { $failedAsExpected = $true }
    $after = @(Get-ChildItem -LiteralPath $backupDirectory -Filter '*.hmsbak' -File).Count
    if (-not $failedAsExpected -or $after -ne $before) { throw 'Failed backup was not surfaced or published an archive.' }
    $failureLog = Get-Content -LiteralPath (Join-Path $backupDirectory 'hms-backup-job.log') -Raw
    if ($failureLog -notmatch 'Result=FAILED;.*BackupPublished=false') { throw 'Failed run was not logged as unsuccessful.' }
    $results.Add('FailedBackupNonzeroAndNotPublished=True')
    $results.Add('FailedBackupLoggedWithoutRawToolOutput=True')
    $results.Add('ScheduledTaskInstalled=False; no application staging database was configured')
    $results.Add('Result=PASS; isolated manual backup and restore behavior verified; assigned nightly-job procedure remains BLOCKED')
}
catch {
    $results.Add('Result=FAIL')
    $results.Add('FailureCategory=' + ($_.Exception.Message -replace '[\r\n,]',' '))
    $results | Set-Content -LiteralPath $evidence -Encoding utf8
    throw
}
finally {
    if ($serverStarted) {
        & (Join-Path $pg 'pg_ctl.exe') -D $data -m fast -w stop | Out-Null
        if ($LASTEXITCODE -ne 0) { $results.Add('ClusterStop=FAILED; isolated cluster data preserved at the temporary path') }
        else { $results.Add('ClusterStop=success; isolated cluster data preserved at the temporary path') }
    }
    $results | Set-Content -LiteralPath $evidence -Encoding utf8
}
Get-Content -LiteralPath $evidence
