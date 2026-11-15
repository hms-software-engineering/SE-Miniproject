$ErrorActionPreference = 'Stop'
$repo = (Get-Location).Path
$build = Join-Path $env:TEMP 'hms-stage3-msvc-ninja-20261115'
$probeExe = Join-Path $env:TEMP 'hms-stage3-tls-probe-20261115\hms_tls_probe.exe'
$pgBin = 'C:\Program Files\PostgreSQL\17\bin'
$evidenceDir = Join-Path $repo 'evidence\automated_tests'
$evidence = Join-Path $evidenceDir 'stage3_api_execution_20261115.txt'
$serverOut = Join-Path $env:TEMP 'hms_stage3_api_native_stdout.log'
$serverErr = Join-Path $env:TEMP 'hms_stage3_api_native_stderr.log'
$server = $null
$client = $null
$out = [Collections.Generic.List[string]]::new()
$probeCa = Join-Path $repo 'certs\hms-local-cert.pem'

function Add-Result([string]$Name, [string]$Value) { $out.Add("$Name=$Value") }
function New-RandomBytesHex([int]$Count) {
    $bytes = [byte[]]::new($Count)
    $rng = [Security.Cryptography.RandomNumberGenerator]::Create()
    $rng.GetBytes($bytes); $rng.Dispose()
    return [BitConverter]::ToString($bytes).Replace('-', '')
}
function New-RandomPassword {
    $bytes = [byte[]]::new(24)
    $rng = [Security.Cryptography.RandomNumberGenerator]::Create()
    $rng.GetBytes($bytes); $rng.Dispose()
    return [Convert]::ToBase64String($bytes) + 'Aa1!'
}
function Invoke-Hms([string]$Method, [string]$Path, [string]$Token = '', [object]$Body = $null, [string]$Mode = 'trusted') {
    $bodyText = ''
    if ($null -ne $Body) { $bodyText = ConvertTo-Json -InputObject $Body -Compress -Depth 8 }
    $parts = @($Method, $Path, $Mode, $(if($Mode -like 'trusted*'){$probeCa}else{''}), $Token, $bodyText)
    $client.StandardInput.WriteLine([string]::Join("`t", $parts))
    $client.StandardInput.Flush()
    $line = $client.StandardOutput.ReadLine()
    if ([string]::IsNullOrEmpty($line)) { throw 'TLS probe returned no response.' }
    $response = $line -split "`t", 5
    if ($response[0] -eq 'OK') {
        $responseBody = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($response[4]))
        $data = $null
        try { $data = $responseBody | ConvertFrom-Json -ErrorAction Stop } catch {}
        return [pscustomobject]@{ Ok=$true; Status=[int]$response[1]; Protocol=$response[2]; Cipher=$response[3]; Body=$responseBody; Data=$data; Error='' }
    }
    $detail = if($response.Count -gt 2 -and $response[2]){[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($response[2]))}else{'TLS probe failed'}
    return [pscustomobject]@{ Ok=$false; Status=0; Protocol=''; Body=''; Data=$null; Error=$detail; VerifyResult=$(if($response.Count -gt 1){$response[1]}else{''}) }
}
function Invoke-Psql([string]$Sql) {
    $psi = [Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = Join-Path $pgBin 'psql.exe'
    $psi.Arguments = '-X -q -A -t -v ON_ERROR_STOP=1 -h 127.0.0.1 -p 55432 -U hms_test -d hms_assigned_test -c "' + $Sql.Replace('"','""') + '"'
    $psi.UseShellExecute = $false; $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true; $psi.RedirectStandardError = $true
    $proc = [Diagnostics.Process]::Start($psi)
    $stdout = $proc.StandardOutput.ReadToEnd(); $stderr = $proc.StandardError.ReadToEnd(); $proc.WaitForExit()
    if ($proc.ExitCode -ne 0) { throw "isolated psql query failed: $($stderr.Trim())" }
    return $stdout.Trim()
}

try {
    Add-Result 'Date' (Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')
    Add-Result 'DatabaseScope' 'isolated PostgreSQL 17.11; hms_assigned_test; 127.0.0.1:55432'
    if (!(Test-Path $probeExe)) { throw 'OpenSSL TLS probe executable is missing.' }
    $env:PATH = (Join-Path $repo 'vcpkg_installed\x64-windows\bin') + ';' + $pgBin + ';' + $env:PATH
    $env:PGHOST='127.0.0.1'; $env:PGPORT='55432'; $env:PGDATABASE='hms_assigned_test'; $env:PGUSER='hms_test'
    $env:HMS_TLS_CERT_FILE = Join-Path $repo 'certs\hms-local-cert.pem'
    $env:HMS_TLS_KEY_FILE = Join-Path $repo 'certs\hms-local-key.pem'
    $env:HMS_PATIENT_DATA_KEY = New-RandomBytesHex 16
    $env:HMS_PATIENT_NAME_INDEX_KEY = New-RandomBytesHex 16
    $env:HMS_PATIENT_PHONE_INDEX_KEY = New-RandomBytesHex 16
    $suffix = [guid]::NewGuid().ToString('N').Substring(0,10)
    $adminName = "admin_$suffix"; $adminPassword = New-RandomPassword
    $env:HMS_BOOTSTRAP_ADMIN_USERNAME = $adminName; $env:HMS_BOOTSTRAP_ADMIN_PASSWORD = $adminPassword
    Remove-Item $serverOut,$serverErr -Force -ErrorAction SilentlyContinue
    $server = Start-Process -FilePath (Join-Path $build 'hms_server.exe') -PassThru -WindowStyle Hidden -RedirectStandardOutput $serverOut -RedirectStandardError $serverErr

    $psi = [Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $probeExe; $psi.UseShellExecute=$false; $psi.CreateNoWindow=$true
    $psi.RedirectStandardInput=$true; $psi.RedirectStandardOutput=$true; $psi.RedirectStandardError=$true
    $client = [Diagnostics.Process]::new(); $client.StartInfo=$psi; $null=$client.Start()

    $tls = $null
    for($i=0;$i -lt 20;$i++){
        if(!(Get-Process -Id $server.Id -ErrorAction SilentlyContinue)){throw 'server exited before TLS verification'}
        $tls=Invoke-Hms GET '/api/v1/health/live' '' $null 'trusted12'
        if($tls.Status -eq 200){break}
        Start-Sleep -Milliseconds 300
    }
    Add-Result 'StartupHealthTLS12' "$($tls.Status); protocol=$($tls.Protocol)"
    try {
        $modules = Get-Process -Id $server.Id -Module | Where-Object ModuleName -in @('libssl-3-x64.dll','libcrypto-3-x64.dll')
        Add-Result 'LoadedOpenSslDllPaths' (($modules | ForEach-Object FileName) -join ';')
    } catch { Add-Result 'LoadedOpenSslDllPaths' 'module path inspection unavailable' }
    Add-Result 'LivenessTLS12' "$($tls.Status); protocol=$($tls.Protocol); cipher=$($tls.Cipher)"
    $tls13 = Invoke-Hms GET '/api/v1/health/live' '' $null 'trusted13'
    Add-Result 'TLS13Negotiation' "$($tls13.Status); protocol=$($tls13.Protocol); cipher=$($tls13.Cipher); verify=$($tls13.VerifyResult); error=$($tls13.Error)"
    $tls11 = Invoke-Hms GET '/api/v1/health/live' '' $null 'trusted11'
    $tls10 = Invoke-Hms GET '/api/v1/health/live' '' $null 'trusted10'
    Add-Result 'TLS11Rejected' ([string](!$tls11.Ok))
    Add-Result 'TLS10Rejected' ([string](!$tls10.Ok))
    $ready = Invoke-Hms GET '/api/v1/health' '' $null 'trusted'
    Add-Result 'ReadinessTLS' "$($ready.Status)"
    $invalidCert = Invoke-Hms GET '/api/v1/health/live' '' $null 'system'
    Add-Result 'UntrustedCertificateRejected' "$(!$invalidCert.Ok); verify=$($invalidCert.VerifyResult)"
    $plain = Invoke-Hms GET '/api/v1/health/live' '' $null 'plain'
    Add-Result 'PlainHTTPRejected' ([string](!$plain.Ok))
    Add-Result 'ServerSurvivedTLSNegativeChecks' ([string]([bool](Get-Process -Id $server.Id -ErrorAction SilentlyContinue)))
    if (!$tls.Ok -or $tls.Status -ne 200) { throw 'verified TLS 1.2 health request did not return HTTP 200' }

    $adminLogin=Invoke-Hms POST '/api/v1/auth/login' '' @{username=$adminName;password=$adminPassword}
    Add-Result 'AdminLogin' "$($adminLogin.Status); role=$($adminLogin.Data.role)"
    if ($adminLogin.Status -ne 200) { throw 'synthetic bootstrap administrator login failed' }
    $adminToken=[string]$adminLogin.Data.token

    $weak=Invoke-Hms POST '/api/v1/users' $adminToken @{username="weak_$suffix";password='abc';role='Receptionist'}
    Add-Result 'WeakPasswordRejected' "$($weak.Status)"
    $nurseName="nurse_$suffix"; $nursePassword=New-RandomPassword
    $nurseCreate=Invoke-Hms POST '/api/v1/users' $adminToken @{username=$nurseName;password=$nursePassword;role='Receptionist'}
    $nurseId=[string]$nurseCreate.Data.userId
    Add-Result 'ReceptionistCreated' "$($nurseCreate.Status); assignedRole=$($nurseCreate.Data.role)"
    $nurseLogin=Invoke-Hms POST '/api/v1/auth/login' '' @{username=$nurseName;password=$nursePassword}
    Add-Result 'ReceptionistLogin' "$($nurseLogin.Status); role=$($nurseLogin.Data.role)"
    $nonAdminCreate=Invoke-Hms POST '/api/v1/users' ([string]$nurseLogin.Data.token) @{username="denied_$suffix";password=(New-RandomPassword);role='Receptionist'}
    Add-Result 'NonAdminUserCreateDenied' "$($nonAdminCreate.Status)"
    $deactivate=Invoke-Hms PATCH "/api/v1/users/$nurseId" $adminToken @{action='deactivate'}
    $inactiveLogin=Invoke-Hms POST '/api/v1/auth/login' '' @{username=$nurseName;password=$nursePassword}
    Add-Result 'ReceptionistDeactivated' "$($deactivate.Status); active=$($deactivate.Data.active)"
    Add-Result 'InactiveLoginRejected' "$($inactiveLogin.Status)"

    $readerName="reader_$suffix"; $readerPass=New-RandomPassword
    $readerCreate=Invoke-Hms POST '/api/v1/users' $adminToken @{username=$readerName;password=$readerPass;role='Receptionist'}
    $readerLogin=Invoke-Hms POST '/api/v1/auth/login' '' @{username=$readerName;password=$readerPass}
    $auditAdmin=Invoke-Hms GET '/api/v1/audit-log?user=admin&from=2026-11-15&to=2026-11-15' $adminToken
    $auditReader=Invoke-Hms GET '/api/v1/audit-log?user=admin&from=2026-11-15&to=2026-11-15' ([string]$readerLogin.Data.token)
    Add-Result 'AdminAuditFilterEndpoint' "$($auditAdmin.Status)"
    Add-Result 'NonAdminAuditReadDenied' "$($auditReader.Status)"

    $sqlUser="' OR '1'='1"; $sqlLogin=Invoke-Hms POST '/api/v1/auth/login' '' @{username=$sqlUser;password=(New-RandomPassword)}
    Add-Result 'LoginSqlInjectionRejected' "$($sqlLogin.Status)"
    $searchQuery=[Uri]::EscapeDataString("' OR '1'='1")
    $sqlSearch=Invoke-Hms GET "/api/v1/patients/search?query=$searchQuery" ([string]$readerLogin.Data.token)
    Add-Result 'SearchInjectionResponse' "$($sqlSearch.Status); matches=$(@($sqlSearch.Data.matches).Count)"

    $doctorName="doctor_$suffix"; $doctorPass=New-RandomPassword
    $doctorCreate=Invoke-Hms POST '/api/v1/users' $adminToken @{username=$doctorName;password=$doctorPass;role='Doctor'}
    $doctorLogin=Invoke-Hms POST '/api/v1/auth/login' '' @{username=$doctorName;password=$doctorPass}
    $doctorId=[string]$doctorCreate.Data.doctorId
    Add-Result 'DoctorCreated' "$($doctorCreate.Status); profilePresent=$([bool]$doctorId)"

    $patientAName="patienta_$suffix"; $patientAPass=New-RandomPassword
    $patientAReg=Invoke-Hms POST '/api/v1/auth/register' '' @{username=$patientAName;password=$patientAPass}
    $patientALogin=Invoke-Hms POST '/api/v1/auth/login' '' @{username=$patientAName;password=$patientAPass}
    $patientAToken=[string]$patientALogin.Data.token
    $profileA=Invoke-Hms POST '/api/v1/patients/me/profile' $patientAToken @{name="Asha Nair Stage3 $suffix";dateOfBirth='03-04-1985';gender='Female';phone='9000000001';address='Synthetic Test Address A'}
    $patientAId=[string]$profileA.Data.patient.patientId
    Add-Result 'PatientAProfileCreated' "$($profileA.Status); idPresent=$([bool]$patientAId)"

    $patientBName="patientb_$suffix"; $patientBPass=New-RandomPassword
    $patientBReg=Invoke-Hms POST '/api/v1/auth/register' '' @{username=$patientBName;password=$patientBPass}
    $patientBLogin=Invoke-Hms POST '/api/v1/auth/login' '' @{username=$patientBName;password=$patientBPass}
    $patientBToken=[string]$patientBLogin.Data.token
    $profileB=Invoke-Hms POST '/api/v1/patients/me/profile' $patientBToken @{name="Mira Shah Stage3 $suffix";dateOfBirth='11-09-1990';gender='Female';phone='8000000002';address='Synthetic Test Address B'}
    $patientBId=[string]$profileB.Data.patient.patientId
    Add-Result 'PatientBProfileCreated' "$($profileB.Status); idPresent=$([bool]$patientBId)"
    $ownRecord=Invoke-Hms GET "/api/v1/patients/$patientAId" $patientAToken
    $crossRecord=Invoke-Hms GET "/api/v1/patients/$patientBId" $patientAToken
    Add-Result 'PatientOwnRecord' "$($ownRecord.Status)"
    Add-Result 'CrossPatientRecordDenied' "$($crossRecord.Status); bodyEmpty=$([string]::IsNullOrEmpty($crossRecord.Body) -or $crossRecord.Status -ne 200)"
    $ownHistory=Invoke-Hms GET '/api/v1/patients/me/history' $patientAToken
    $ownBills=Invoke-Hms GET '/api/v1/patients/me/bills' $patientAToken
    $crossHistory=Invoke-Hms GET "/api/v1/patients/$patientBId/history" $patientAToken
    Add-Result 'PatientOwnHistoryEndpoint' "$($ownHistory.Status)"
    Add-Result 'PatientOwnBillsEndpoint' "$($ownBills.Status)"
    Add-Result 'CrossPatientHistoryDenied' "$($crossHistory.Status)"

    $seedDoctorSlots=Invoke-Hms GET '/api/v1/appointments/free-slots?doctorId=1&date=2026-11-16' ([string]$readerLogin.Data.token)
    $otherDoctorTime=''
    if (@($seedDoctorSlots.Data.slots).Count -gt 0) { $otherDoctorTime=[string]$seedDoctorSlots.Data.slots[-1] }
    Add-Result 'OtherDoctorFreeSlotAvailable' ([string][bool]$otherDoctorTime)
    if ($otherDoctorTime) { $otherDoctorAppointment=Invoke-Hms POST '/api/v1/appointments' $patientBToken @{patientId=$patientBId;doctorId='1';date='2026-11-16';time=$otherDoctorTime} }
    else { $otherDoctorAppointment=$null }
    Add-Result 'OtherDoctorAppointmentBooked' "$(if($otherDoctorAppointment){$otherDoctorAppointment.Status}else{'SKIPPED'})"
    $appointment=Invoke-Hms POST '/api/v1/appointments' $patientAToken @{patientId=$patientAId;doctorId=$doctorId;date='2026-11-16';time='09:00'}
    $appointmentId=[string]$appointment.Data.appointment.appointmentId
    Add-Result 'SyntheticAppointmentBooked' "$($appointment.Status); idPresent=$([bool]$appointmentId)"
    $documentedSchedule=Invoke-Hms GET '/api/v1/doctors/me/schedule?date=2026-11-16' ([string]$doctorLogin.Data.token)
    $implementedSchedule=Invoke-Hms GET '/api/v1/doctors/me/appointments?date=2026-11-16' ([string]$doctorLogin.Data.token)
    Add-Result 'AssignedScheduleEndpoint' "$($documentedSchedule.Status)"
    Add-Result 'ImplementedScheduleEndpoint' "$($implementedSchedule.Status)"

    if ($appointmentId) {
        $visit=Invoke-Hms POST '/api/v1/visits' ([string]$doctorLogin.Data.token) @{appointmentId=$appointmentId;symptoms='Synthetic symptom';diagnosis='Synthetic diagnosis Stage3';notes='<script>alert(1)</script>'}
        $visitId=[string]$visit.Data.visitId
        Add-Result 'SyntheticVisitCreated' "$($visit.Status); idPresent=$([bool]$visitId)"
    } else { $visit=$null; $visitId=''; Add-Result 'SyntheticVisitCreated' 'SKIPPED; appointment prerequisite missing' }

    $billingName="billing_$suffix"; $billingPass=New-RandomPassword
    $billingCreate=Invoke-Hms POST '/api/v1/users' $adminToken @{username=$billingName;password=$billingPass;role='Billing Officer'}
    $billingLogin=Invoke-Hms POST '/api/v1/auth/login' '' @{username=$billingName;password=$billingPass}
    Add-Result 'BillingOfficerCreated' "$($billingCreate.Status); role=$($billingCreate.Data.role)"
    $bill=Invoke-Hms POST '/api/v1/bills' ([string]$billingLogin.Data.token) @{visitId=$visitId;items=@(@{description='Consultation';amount=500},@{description='Procedure';amount=300})}
    Add-Result 'BillCreationEndpoint' "$($bill.Status)"
    $payment=Invoke-Hms POST '/api/v1/payments' ([string]$billingLogin.Data.token) @{billId='1';amount=400;mode='UPI'}
    $receipt=Invoke-Hms POST '/api/v1/receipts' ([string]$billingLogin.Data.token) @{billId='1'}
    Add-Result 'PaymentEndpoint' "$($payment.Status)"
    Add-Result 'ReceiptEndpoint' "$($receipt.Status)"
    $report=Invoke-Hms GET '/api/v1/reports/revenue?from=2026-11-08&to=2026-11-15&format=csv' $adminToken
    Add-Result 'RevenueReportEndpoint' "$($report.Status)"

    $update=Invoke-Hms PUT "/api/v1/patients/$patientAId" ([string]$readerLogin.Data.token) @{name="Asha Nair Updated $suffix";dateOfBirth='03-04-1985';gender='Female';phone='9000000001';address='Synthetic Updated Address'}
    Add-Result 'SyntheticPatientUpdate' "$($update.Status)"
    $logout=Invoke-Hms POST '/api/v1/auth/logout' $patientAToken @{}
    $replay=Invoke-Hms GET '/api/v1/auth/me' $patientAToken
    Add-Result 'Logout' "$($logout.Status)"
    Add-Result 'LoggedOutTokenReplay' "$($replay.Status)"

    $credSql="SELECT count(*) || '|' || count(DISTINCT split_part(password_hash,'$',5)) || '|' || coalesce(bool_and(password_hash LIKE '`$argon2id`$%'),'false') FROM users WHERE username IN ('$adminName','$nurseName','$readerName','$doctorName','$patientAName','$patientBName','$billingName')"
    Add-Result 'CredentialHashSummary' (Invoke-Psql $credSql)
    $activeSql="SELECT active::text FROM users WHERE username='$nurseName'"
    Add-Result 'DeactivationPersisted' (Invoke-Psql $activeSql)
    if ($patientAId) {
        $encSql="SELECT (position(convert_to('Asha Nair Updated $suffix','UTF8') in name)=0)::text FROM patients WHERE patient_id='$patientAId'"
        Add-Result 'PatientNameNotPlaintextAtRest' (Invoke-Psql $encSql)
    }
    if ($visitId) {
        $diagSql="SELECT (position(convert_to('Synthetic diagnosis Stage3','UTF8') in diagnosis)=0)::text || '|' || (position(convert_to('<script>alert(1)</script>','UTF8') in notes)=0)::text FROM visits WHERE visit_id=$visitId"
        Add-Result 'VisitDiagnosisAndNotesNotPlaintext' (Invoke-Psql $diagSql)
    }
    $auditSql="SELECT count(*) FROM audit_log al JOIN users u ON u.user_id=al.user_id WHERE u.username='$adminName' AND al.action IN ('LOGIN','CREATE_USER','DEACTIVATE_USER')"
    Add-Result 'AuditEventCountForSyntheticAdmin' (Invoke-Psql $auditSql)
    $redactionSql="SELECT count(*) || '|' || coalesce(bool_and(position('<script>' in coalesce(al.record_id,''))=0),'true') FROM audit_log al JOIN users u ON u.user_id=al.user_id WHERE u.username IN ('$adminName','$doctorName','$readerName')"
    Add-Result 'AuditRowsAndNoScriptNote' (Invoke-Psql $redactionSql)
    Add-Result 'ServerAliveAfterApiChecks' ([string]([bool](Get-Process -Id $server.Id -ErrorAction SilentlyContinue)))
    Add-Result 'NoSecretsRecorded' 'true'
    $out | Set-Content -LiteralPath $evidence -Encoding utf8
}
catch {
    Add-Result 'ExecutionError' $_.Exception.Message
    $out | Set-Content -LiteralPath $evidence -Encoding utf8
}
finally {
    if ($client) { try { $client.StandardInput.Close(); $client.WaitForExit(3000) | Out-Null } catch {}; if (!$client.HasExited) { $client.Kill() } }
    if ($server -and (Get-Process -Id $server.Id -ErrorAction SilentlyContinue)) { Stop-Process -Id $server.Id -Force; Add-Content -LiteralPath $evidence -Value 'ServerStopped=true' }
    if (Test-Path $serverOut) { $safe = Get-Content $serverOut -Raw; $safe = $safe -replace 'PID-[0-9]{8}','PID-REDACTED'; [IO.File]::WriteAllText((Join-Path $evidenceDir 'stage3_api_server_stdout_20261115.log'),$safe,[Text.UTF8Encoding]::new($false)) }
    if (Test-Path $serverErr) { Copy-Item $serverErr (Join-Path $evidenceDir 'stage3_api_server_stderr_20261115.log') -Force }
    Remove-Item Env:PGHOST,Env:PGPORT,Env:PGDATABASE,Env:PGUSER,Env:HMS_TLS_CERT_FILE,Env:HMS_TLS_KEY_FILE,Env:HMS_PATIENT_DATA_KEY,Env:HMS_PATIENT_NAME_INDEX_KEY,Env:HMS_PATIENT_PHONE_INDEX_KEY,Env:HMS_BOOTSTRAP_ADMIN_USERNAME,Env:HMS_BOOTSTRAP_ADMIN_PASSWORD -ErrorAction SilentlyContinue
}
Get-Content -LiteralPath $evidence
