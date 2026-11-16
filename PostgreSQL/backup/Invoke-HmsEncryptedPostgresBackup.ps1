param(
    [Parameter(Mandatory=$true)][ValidateSet('Backup','Restore')][string]$Mode,
    [Parameter(Mandatory=$true)][string]$PgBinPath,
    [Parameter(Mandatory=$true)][string]$DbHost,
    [Parameter(Mandatory=$true)][ValidateRange(1,65535)][int]$DbPort,
    [Parameter(Mandatory=$true)][string]$Database,
    [Parameter(Mandatory=$true)][string]$Username,
    [Parameter(Mandatory=$true)][string]$BackupDirectory,
    [Parameter(Mandatory=$true)][string]$DatabaseDataDirectory,
    [string]$BackupPath,
    [string]$PgPassFile,
    [string]$RestoreDatabase
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Security
$chunkBytes = 2MB
$fixedEntropy = [Text.Encoding]::UTF8.GetBytes('HMS-POSTGRES-DPAPI-MANIFEST-v1')
$magic = [Text.Encoding]::ASCII.GetBytes('HMSBAK01')
$logPath = Join-Path $BackupDirectory 'hms-backup-job.log'
$plainPath = $null
$temporaryArchive = $null
$tempDirectory = $null
$savedEnvironment = @{}

function Write-JobLog([string]$line) {
    [IO.File]::AppendAllText($logPath, $line + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))
}

function Assert-OutsideDataDirectory([string]$candidate) {
    $full = [IO.Path]::GetFullPath($candidate).TrimEnd('\') + '\'
    $data = [IO.Path]::GetFullPath($DatabaseDataDirectory).TrimEnd('\') + '\'
    if ($full.StartsWith($data, [StringComparison]::OrdinalIgnoreCase) -or
        $data.StartsWith($full, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Backup storage must be a separate directory from PostgreSQL data.'
    }
}

function Write-EncryptedArchive([string]$source, [string]$destination) {
    $sourceInfo = Get-Item -LiteralPath $source
    $hash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    $count = [int][Math]::Ceiling($sourceInfo.Length / [double]$chunkBytes)
    $manifestJson = ConvertTo-Json -Compress -InputObject @{
        length = [long]$sourceInfo.Length
        chunks = $count
        sha256 = $hash
    }
    $manifest = [Text.Encoding]::UTF8.GetBytes($manifestJson)
    $protectedManifest = [Security.Cryptography.ProtectedData]::Protect(
        $manifest, $fixedEntropy,
        [Security.Cryptography.DataProtectionScope]::CurrentUser)
    $inputStream = [IO.File]::OpenRead($source)
    $outputStream = [IO.File]::Open($destination, [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write, [IO.FileShare]::None)
    $writer = [IO.BinaryWriter]::new($outputStream)
    try {
        $writer.Write($magic)
        $writer.Write([int]$protectedManifest.Length)
        $writer.Write($protectedManifest)
        $buffer = [byte[]]::new($chunkBytes)
        for ($index = 0; $index -lt $count; $index++) {
            $read = $inputStream.Read($buffer, 0, $buffer.Length)
            if ($read -le 0) { throw 'Backup source ended before its manifest length.' }
            $chunk = [byte[]]::new($read)
            [Array]::Copy($buffer, $chunk, $read)
            $entropy = [Text.Encoding]::UTF8.GetBytes("HMS-POSTGRES-DPAPI-CHUNK-v1:$index")
            $protectedChunk = [Security.Cryptography.ProtectedData]::Protect(
                $chunk, $entropy,
                [Security.Cryptography.DataProtectionScope]::CurrentUser)
            $writer.Write([int]$protectedChunk.Length)
            $writer.Write($protectedChunk)
        }
        if ($inputStream.ReadByte() -ne -1) { throw 'Backup source changed during encryption.' }
        $writer.Write([int]0)
        $writer.Flush()
    } finally {
        $writer.Dispose()
        $inputStream.Dispose()
    }
    return @{ PlainBytes = [long]$sourceInfo.Length; PlainSha256 = $hash }
}

function Restore-EncryptedArchive([string]$archive, [string]$destination) {
    $stream = [IO.File]::OpenRead($archive)
    $reader = [IO.BinaryReader]::new($stream)
    $output = $null
    try {
        $readMagic = $reader.ReadBytes($magic.Length)
        if ([Convert]::ToBase64String($readMagic) -ne [Convert]::ToBase64String($magic)) {
            throw 'Encrypted backup header is invalid.'
        }
        $manifestLength = $reader.ReadInt32()
        if ($manifestLength -lt 1 -or $manifestLength -gt 1048576) {
            throw 'Encrypted backup manifest length is invalid.'
        }
        $protectedManifest = $reader.ReadBytes($manifestLength)
        if ($protectedManifest.Length -ne $manifestLength) { throw 'Encrypted backup manifest is truncated.' }
        $manifestBytes = [Security.Cryptography.ProtectedData]::Unprotect(
            $protectedManifest, $fixedEntropy,
            [Security.Cryptography.DataProtectionScope]::CurrentUser)
        $manifest = [Text.Encoding]::UTF8.GetString($manifestBytes) | ConvertFrom-Json
        if ($manifest.chunks -lt 0 -or $manifest.length -lt 0 -or
            $manifest.sha256 -notmatch '^[0-9A-Fa-f]{64}$') {
            throw 'Encrypted backup manifest values are invalid.'
        }
        $output = [IO.File]::Open($destination, [IO.FileMode]::CreateNew,
            [IO.FileAccess]::Write, [IO.FileShare]::None)
        for ($index = 0; $index -lt [int]$manifest.chunks; $index++) {
            $length = $reader.ReadInt32()
            if ($length -lt 1 -or $length -gt ($chunkBytes + 65536)) {
                throw 'Encrypted backup chunk length is invalid.'
            }
            $payload = $reader.ReadBytes($length)
            if ($payload.Length -ne $length) { throw 'Encrypted backup chunk is truncated.' }
            $entropy = [Text.Encoding]::UTF8.GetBytes("HMS-POSTGRES-DPAPI-CHUNK-v1:$index")
            $clear = [Security.Cryptography.ProtectedData]::Unprotect(
                $payload, $entropy,
                [Security.Cryptography.DataProtectionScope]::CurrentUser)
            $output.Write($clear, 0, $clear.Length)
        }
        if ($reader.ReadInt32() -ne 0 -or $stream.Position -ne $stream.Length) {
            throw 'Encrypted backup has trailing or malformed data.'
        }
        $output.Flush()
        $output.Dispose(); $output = $null
        $info = Get-Item -LiteralPath $destination
        $hash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        if ([long]$info.Length -ne [long]$manifest.length -or
            $hash -ine [string]$manifest.sha256) {
            throw 'Restored backup stream failed length or SHA256 validation.'
        }
        return @{ PlainBytes = [long]$info.Length; PlainSha256 = $hash }
    } finally {
        if ($output) { $output.Dispose() }
        $reader.Dispose()
    }
}

function Quote-NativeArgument([string]$value) {
    if ($value -notmatch '[\s"]') { return $value }
    $escaped = [Regex]::Replace($value, '(\\*)"', '$1$1\"')
    $escaped = [Regex]::Replace($escaped, '(\\+)$', '$1$1')
    return '"' + $escaped + '"'
}

function Invoke-PgTool([string]$tool, [string[]]$arguments) {
    $stdout = Join-Path $tempDirectory 'tool.stdout'
    $stderr = Join-Path $tempDirectory 'tool.stderr'
    $quoted = ($arguments | ForEach-Object { Quote-NativeArgument $_ }) -join ' '
    $process = Start-Process -FilePath (Join-Path $PgBinPath $tool) `
        -ArgumentList $quoted -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if ($process.ExitCode -ne 0) {
        throw "$tool failed with exit code $($process.ExitCode). Its output is withheld from the job log."
    }
    if (Test-Path $stderr) { Remove-Item -LiteralPath $stderr -Force }
    if (Test-Path $stdout) { Remove-Item -LiteralPath $stdout -Force }
}

try {
    if (-not (Test-Path -LiteralPath $PgBinPath -PathType Container)) { throw 'PostgreSQL client tools directory is unavailable.' }
    if (-not (Test-Path -LiteralPath $DatabaseDataDirectory -PathType Container)) { throw 'Configured PostgreSQL data directory does not exist.' }
    if (-not (Test-Path -LiteralPath $BackupDirectory -PathType Container)) {
        New-Item -ItemType Directory -Path $BackupDirectory -ErrorAction Stop | Out-Null
    }
    Assert-OutsideDataDirectory $BackupDirectory
    if ($Mode -eq 'Restore') {
        if (-not $RestoreDatabase -or $RestoreDatabase -eq $Database -or
            $RestoreDatabase -notmatch '^hms_stage6_restore_[A-Za-z0-9_]+$') {
            throw 'Restore requires a separate disposable database named hms_stage6_restore_*.'
        }
        if (-not $BackupPath -or -not (Test-Path -LiteralPath $BackupPath -PathType Leaf)) { throw 'Encrypted backup file does not exist.' }
    } else {
        if ($RestoreDatabase) { throw 'RestoreDatabase is valid only in Restore mode.' }
        if (-not $BackupPath) {
            $stamp = [DateTimeOffset]::UtcNow.ToString('yyyyMMddTHHmmssZ')
            $safeDatabase = $Database -replace '[^A-Za-z0-9_-]','_'
            $BackupPath = Join-Path $BackupDirectory "hms-$safeDatabase-$stamp.hmsbak"
        }
        if (Test-Path -LiteralPath $BackupPath) { throw 'BackupPath already exists; refusing to overwrite it.' }
    }
    if ($PgPassFile) {
        if (-not (Test-Path -LiteralPath $PgPassFile -PathType Leaf)) { throw 'Configured PGPASSFILE does not exist.' }
        $acl = Get-Acl -LiteralPath $PgPassFile
        $allowedSids = @(
            [Security.Principal.WindowsIdentity]::GetCurrent().User.Value,
            ([Security.Principal.SecurityIdentifier]::new([Security.Principal.WellKnownSidType]::LocalSystemSid,$null)).Value,
            ([Security.Principal.SecurityIdentifier]::new([Security.Principal.WellKnownSidType]::BuiltinAdministratorsSid,$null)).Value
        )
        foreach ($rule in $acl.Access) {
            if ($rule.AccessControlType -ne [Security.AccessControl.AccessControlType]::Allow) { continue }
            try { $sid = $rule.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value }
            catch { throw 'Could not validate PGPASSFILE access control.' }
            if ($sid -notin $allowedSids -and $rule.FileSystemRights.ToString() -match 'Read|Write|Modify|FullControl') {
                throw 'PGPASSFILE grants read or write access beyond its owner, SYSTEM, and Administrators.'
            }
        }
        $savedEnvironment.PGPASSFILE = $env:PGPASSFILE
        $env:PGPASSFILE = [IO.Path]::GetFullPath($PgPassFile)
    }
    $savedEnvironment.PGAPPNAME = $env:PGAPPNAME
    $env:PGAPPNAME = 'HMS-Daily-Encrypted-Backup'
    $tempDirectory = Join-Path ([IO.Path]::GetTempPath()) ('hms-backup-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $tempDirectory -ErrorAction Stop | Out-Null
    $started = [DateTimeOffset]::UtcNow
    Write-JobLog ("StartedUtc={0:o}; Mode={1}; Database={2}" -f $started,$Mode,$Database)

    if ($Mode -eq 'Backup') {
        $plainPath = Join-Path $tempDirectory 'database.pgdump'
        $temporaryArchive = $BackupPath + '.pending-' + [guid]::NewGuid().ToString('N')
        Invoke-PgTool 'pg_dump.exe' @('-Fc','-h',$DbHost,'-p',[string]$DbPort,'-U',$Username,'-d',$Database,'-f',$plainPath)
        if (-not (Test-Path -LiteralPath $plainPath -PathType Leaf) -or (Get-Item $plainPath).Length -lt 1) {
            throw 'pg_dump returned success without producing a non-empty archive.'
        }
        $result = Write-EncryptedArchive $plainPath $temporaryArchive
        Move-Item -LiteralPath $temporaryArchive -Destination $BackupPath -ErrorAction Stop
        $encryptedHash = (Get-FileHash -LiteralPath $BackupPath -Algorithm SHA256).Hash
        $ended = [DateTimeOffset]::UtcNow
        Write-JobLog ("Result=SUCCESS; FinishedUtc={0:o}; Backup={1}; Bytes={2}; CiphertextSha256={3}" -f $ended,[IO.Path]::GetFileName($BackupPath),$result.PlainBytes,$encryptedHash)
    } else {
        $plainPath = Join-Path $tempDirectory 'database.pgdump'
        $result = Restore-EncryptedArchive $BackupPath $plainPath
        Invoke-PgTool 'pg_restore.exe' @('--exit-on-error','-h',$DbHost,'-p',[string]$DbPort,'-U',$Username,'-d',$RestoreDatabase,$plainPath)
        $ended = [DateTimeOffset]::UtcNow
        Write-JobLog ("Result=RESTORE_SUCCESS; FinishedUtc={0:o}; RestoreDatabase={1}; Bytes={2}; PlainSha256={3}" -f $ended,$RestoreDatabase,$result.PlainBytes,$result.PlainSha256)
    }
}
catch {
    $code = if ($_.Exception.Message -match 'exit code (\d+)') { $Matches[1] } else { 'configuration-or-validation' }
    try { if (Test-Path -LiteralPath $BackupDirectory -PathType Container) {
        Write-JobLog ("Result=FAILED; FailureCode={0}; BackupPublished=false" -f $code)
    }} catch {}
    if ($temporaryArchive -and (Test-Path -LiteralPath $temporaryArchive)) {
        Remove-Item -LiteralPath $temporaryArchive -Force -ErrorAction SilentlyContinue
    }
    throw 'HMS encrypted PostgreSQL backup/restore failed; inspect the sanitized job log and Task Scheduler result.'
}
finally {
    $temporaryFiles = @($plainPath)
    if ($tempDirectory) {
        $temporaryFiles += (Join-Path $tempDirectory 'tool.stdout')
        $temporaryFiles += (Join-Path $tempDirectory 'tool.stderr')
    }
    foreach ($path in $temporaryFiles) {
        if ($path -and (Test-Path -LiteralPath $path)) { Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue }
    }
    if ($tempDirectory -and (Test-Path -LiteralPath $tempDirectory)) {
        Remove-Item -LiteralPath $tempDirectory -Force -Recurse -ErrorAction SilentlyContinue
    }
    foreach ($name in $savedEnvironment.Keys) {
        if ($null -eq $savedEnvironment[$name]) { Remove-Item "Env:$name" -ErrorAction SilentlyContinue }
        else { Set-Item "Env:$name" $savedEnvironment[$name] }
    }
}
