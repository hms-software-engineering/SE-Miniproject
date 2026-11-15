$ErrorActionPreference = 'Stop'
$repo = (Get-Location).Path
$pgBin = 'C:\Program Files\PostgreSQL\17\bin'
$build = Join-Path $env:TEMP 'hms-stage3-msvc-ninja-20261115'
$probeExe = Join-Path $env:TEMP 'hms-stage3-tls-probe-20261115\hms_tls_probe.exe'
$evidence = Join-Path $repo 'evidence\automated_tests\stage3_idle_session_20261115.txt'
$progressFile = Join-Path $repo 'evidence\automated_tests\stage3_idle_session_progress_20261115.log'
$serverOut = Join-Path $env:TEMP 'hms_stage3_idle_server_stdout.log'
$serverErr = Join-Path $env:TEMP 'hms_stage3_idle_server_stderr.log'
$server = $null; $client = $null; $results = [Collections.Generic.List[string]]::new()

function New-HexKey {
    $b=[byte[]]::new(16);$r=[Security.Cryptography.RandomNumberGenerator]::Create();$r.GetBytes($b);$r.Dispose();return [BitConverter]::ToString($b).Replace('-','')
}
function New-Password {
    $b=[byte[]]::new(24);$r=[Security.Cryptography.RandomNumberGenerator]::Create();$r.GetBytes($b);$r.Dispose();return [Convert]::ToBase64String($b)+'Aa1!'
}
function Invoke-LocalApi([string]$Method,[string]$Path,[string]$Token,[object]$Body=$null) {
    $bodyText='';if($null -ne $Body){$bodyText=ConvertTo-Json -InputObject $Body -Compress}
    $line=[string]::Join("`t",@($Method,$Path,'trusted',(Join-Path $repo 'certs\hms-local-cert.pem'),$Token,$bodyText))
    $client.StandardInput.WriteLine($line);$client.StandardInput.Flush()
    $parts=$client.StandardOutput.ReadLine() -split "`t",5
    if($parts[0] -ne 'OK'){return [pscustomobject]@{Status=0;Data=$null;Error=$parts[2]}}
    $raw=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($parts[4]));$data=$null
    try{$data=$raw|ConvertFrom-Json -ErrorAction Stop}catch{}
    return [pscustomobject]@{Status=[int]$parts[1];Data=$data;Error=''}
}

try {
    $env:PATH=(Join-Path $repo 'vcpkg_installed\x64-windows\bin')+';'+$pgBin+';'+$env:PATH
    $env:PGHOST='127.0.0.1';$env:PGPORT='55432';$env:PGDATABASE='hms_assigned_test';$env:PGUSER='hms_test'
    $env:HMS_TLS_CERT_FILE=Join-Path $repo 'certs\hms-local-cert.pem';$env:HMS_TLS_KEY_FILE=Join-Path $repo 'certs\hms-local-key.pem'
    $env:HMS_PATIENT_DATA_KEY=New-HexKey;$env:HMS_PATIENT_NAME_INDEX_KEY=New-HexKey;$env:HMS_PATIENT_PHONE_INDEX_KEY=New-HexKey
    $u='idle_'+[guid]::NewGuid().ToString('N').Substring(0,10);$p=New-Password
    $env:HMS_BOOTSTRAP_ADMIN_USERNAME=$u;$env:HMS_BOOTSTRAP_ADMIN_PASSWORD=$p
    $server=Start-Process -FilePath (Join-Path $build 'hms_server.exe') -PassThru -WindowStyle Hidden -RedirectStandardOutput $serverOut -RedirectStandardError $serverErr
    $psi=[Diagnostics.ProcessStartInfo]::new();$psi.FileName=$probeExe;$psi.UseShellExecute=$false;$psi.CreateNoWindow=$true;$psi.RedirectStandardInput=$true;$psi.RedirectStandardOutput=$true;$psi.RedirectStandardError=$true
    $client=[Diagnostics.Process]::new();$client.StartInfo=$psi;$null=$client.Start()
    $health=$null
    for($i=0;$i -lt 20;$i++){Start-Sleep -Milliseconds 300;$health=Invoke-LocalApi GET '/api/v1/health/live' '';if($health.Status -eq 200){break}}
    if(!$health -or $health.Status -ne 200){throw 'server failed to answer the verified TLS health request'}
    $login=Invoke-LocalApi POST '/api/v1/auth/login' '' @{username=$u;password=$p}
    $results.Add("LoginStatus=$($login.Status)")
    if($login.Status -ne 200){throw 'could not establish isolated idle-expiry session'}
    $token=[string]$login.Data.token
    $start=Get-Date;$end=$start.AddMinutes(10).AddSeconds(5)
    $header=@("Date=$($start.ToString('yyyy-MM-dd HH:mm:ss zzz'))","Scope=isolated PG17 test DB; synthetic bootstrap administrator","Idle interval=10 minutes 5 seconds","No requests made with the token during the idle interval")
    [IO.File]::WriteAllLines($progressFile,[string[]]$header,[Text.UTF8Encoding]::new($false))
    while((Get-Date) -lt $end){
        $remaining=[int][Math]::Ceiling(($end-(Get-Date)).TotalSeconds)
        $sleep=[Math]::Min(60,[Math]::Max(1,$remaining))
        Start-Sleep -Seconds $sleep
        $elapsed=[int]((Get-Date)-$start).TotalSeconds
        Add-Content -LiteralPath $progressFile -Value "ElapsedSeconds=$elapsed"
        Write-Output "Idle expiry observation: $elapsed seconds elapsed."
    }
    $after=Invoke-LocalApi GET '/api/v1/auth/me' $token
    $results.Add("AfterIdleRequestStatus=$($after.Status)")
    $results.Add("ExpectedExpiredSession401=$($after.Status -eq 401)")
    $results.Add('TokenOrPasswordRecorded=false')
    $results.Add('ServerAliveAfterIdleRequest='+[string][bool](Get-Process -Id $server.Id -ErrorAction SilentlyContinue))
    $results | Set-Content -LiteralPath $evidence -Encoding utf8
}
catch {
    $results.Add("ExecutionError=$($_.Exception.Message)")
    $results | Set-Content -LiteralPath $evidence -Encoding utf8
}
finally {
    if($client){try{$client.StandardInput.Close();$client.WaitForExit(3000)|Out-Null}catch{};if(!$client.HasExited){$client.Kill()}}
    if($server -and (Get-Process -Id $server.Id -ErrorAction SilentlyContinue)){Stop-Process -Id $server.Id -Force;Add-Content -LiteralPath $evidence -Value 'ServerStopped=true'}
    if(Test-Path $serverOut){$s=Get-Content $serverOut -Raw;$s=$s -replace 'PID-[0-9]{8}','PID-REDACTED';[IO.File]::WriteAllText((Join-Path $repo 'evidence\automated_tests\stage3_idle_session_server_stdout_20261115.log'),$s,[Text.UTF8Encoding]::new($false))}
    if(Test-Path $serverErr){Copy-Item $serverErr (Join-Path $repo 'evidence\automated_tests\stage3_idle_session_server_stderr_20261115.log') -Force}
    Remove-Item Env:PGHOST,Env:PGPORT,Env:PGDATABASE,Env:PGUSER,Env:HMS_TLS_CERT_FILE,Env:HMS_TLS_KEY_FILE,Env:HMS_PATIENT_DATA_KEY,Env:HMS_PATIENT_NAME_INDEX_KEY,Env:HMS_PATIENT_PHONE_INDEX_KEY,Env:HMS_BOOTSTRAP_ADMIN_USERNAME,Env:HMS_BOOTSTRAP_ADMIN_PASSWORD -ErrorAction SilentlyContinue
}
Get-Content -LiteralPath $evidence
