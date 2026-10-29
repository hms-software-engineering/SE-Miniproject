# HMS-NF-006 PostgreSQL backup and recovery

## Design and status

`hms_backup` is a standalone C++ utility. It runs PostgreSQL's `pg_dump` in custom-archive format, checks the archive with `pg_restore --list`, then streams the archive through OpenSSL AES-256-GCM into a unique encrypted artifact. The format is `HMSBK01\0`, a fresh 12-byte random nonce, ciphertext, and a 16-byte GCM tag. Authentication is checked before a decrypted file is published. A per-directory operating-system lock prevents overlapping backup and restore runs. Temporary files are removed after errors and stale temporary files are cleaned on the next run.

The utility does not start inside API request handling. On Windows, deploy it with Task Scheduler under a dedicated service identity. Configure a task to run `hms_backup.exe run-due` once per hour with “Start the task as soon as possible after a scheduled start is missed” enabled and “Do not start a new instance” as the multiple-instance policy. Set `HMS_BACKUP_INTERVAL_HOURS=23` for the scheduled task to leave a one-hour start margin; manual and default interval is 24 hours. `run-due` checks the timestamp of the last successful backup. A machine that is offline or whose task cannot run for more than the margin can exceed the interval; monitor `hms_backup status` and alert on `backup_due=true` or `FAILURE`.

This repository includes `PostgreSQL/register-backup-task.ps1` to register the recurring Windows task. From an elevated PowerShell prompt, first provision protected environment variables for the dedicated service account, then run:

```powershell
.\PostgreSQL\register-backup-task.ps1 `
  -ExecutablePath 'C:\Program Files\HMS\hms_backup.exe' `
  -RunAsUser 'DOMAIN\hms-backup'
```

The script prompts for that account's task password without placing it in the command line, registers an hourly task with start-when-available, ignore-new-instance, and retry settings, and relies on Windows Task Scheduler to protect the task credentials. This repository does not install the task automatically; scheduling is operational only after an administrator configures the service account's protected environment and starts/monitors the task. Do not put the key in task arguments, a checked-in script, or a shared machine-wide environment when it can be avoided.

## Prerequisites and build

Use PostgreSQL 15 client tools (`pg_dump`, `pg_restore`, and `psql`) matching the server's major version, plus OpenSSL 1.1.1 or newer. The project already links OpenSSL. Build with the repository's normal CMake configuration:

```powershell
cmake -S . -B build
cmake --build build --config Release --target hms_backup_cli
```

The output executable is `build\hms_backup.exe` (or the selected CMake configuration's executable directory). PostgreSQL `bin` must be on the task account's `PATH`.

## Configuration and key handling

Set these variables for the account running the tool:

| Variable | Required | Meaning |
| --- | --- | --- |
| `HMS_BACKUP_DIR` | Yes | Dedicated absolute directory for encrypted archives, status, and short-lived working files. |
| `HMS_BACKUP_KEY` | For backup/restore | Exactly 32 random bytes encoded as 64 hexadecimal characters. Keep it in a secret manager or protected account-scoped secret store. |
| `HMS_BACKUP_INTERVAL_HOURS` | No | Due threshold; default 24. Scheduled hourly task should use 23 as described above. |
| `HMS_BACKUP_RETENTION_DAYS` | No | Retain encrypted artifacts for this many days; default 30. |
| `HMS_BACKUP_PROCESS_TIMEOUT_SECONDS` | No | Maximum runtime per PostgreSQL tool process; default 43,200 seconds (12 hours), maximum 604,800 seconds. |
| `PGHOST`, `PGPORT`, `PGDATABASE`, `PGUSER`, `PGPASSWORD` | As required | Standard libpq connection configuration. Prefer a protected PostgreSQL passfile (`PGPASSFILE`) over `PGPASSWORD` for persistent service credentials. |

Generate a key with a cryptographically secure generator, for example in PowerShell with OpenSSL installed:

```powershell
openssl rand -hex 32
```

Provision the value through the organization's secret manager. Back up the key separately from the artifacts and restrict retrieval to recovery operators. If the key is lost, the encrypted backups are unrecoverable. Rotate by provisioning a new key for future artifacts while retaining the old key under controlled access until every backup encrypted with it expires. This initial format does not embed a key identifier; record key version to artifact date in the secret manager's recovery runbook. Never commit the key or database credentials.

Create a dedicated directory, restrict it to the task identity and recovery administrators, and verify inherited ACLs. For example, from an elevated PowerShell session, replacing the path and account:

```powershell
New-Item -ItemType Directory -Force D:\HMS\Backups | Out-Null
icacls D:\HMS\Backups /inheritance:r
icacls D:\HMS\Backups /grant:r 'DOMAIN\hms-backup:(OI)(CI)F' 'DOMAIN\hms-recovery:(OI)(CI)F' 'SYSTEM:(OI)(CI)F'
```

On POSIX deployments, the utility restricts the configured backup directory to owner-only (`0700`) before creating temporary or final files. On Windows, validate the directory ACL as above because the C++ filesystem API does not establish Windows security descriptors.

`pg_dump` requires a temporary plaintext custom archive before encryption. It is created only inside this protected directory and removed after encryption or failure. A sudden power loss may leave a partial plaintext file until the next run; the next backup removes stale `.partial-*` files. Use BitLocker or equivalent volume encryption and ensure the volume's ACLs remain restricted. SSD and filesystem deletion is not guaranteed to securely erase blocks.

## Run, monitor, and validate

With configuration present, run a manual backup:

```powershell
& 'C:\Program Files\HMS\hms_backup.exe' backup
```

For Task Scheduler, use action executable `C:\Program Files\HMS\hms_backup.exe`, argument `run-due`, the dedicated service identity, hourly trigger, start-when-available, and ignore-new-instance policy. Set the variables in that identity's protected runtime configuration. Confirm task history shows successful starts and inspect:

```powershell
& 'C:\Program Files\HMS\hms_backup.exe' status
Get-ChildItem $env:HMS_BACKUP_DIR -Filter '*.hmsbak' | Sort-Object LastWriteTime -Descending
Get-Content (Join-Path $env:HMS_BACKUP_DIR 'backup-status.txt')
```

`backup-status.txt` records the current result; `backup-last-success.txt` independently preserves the last successful UTC epoch and artifact name, even after a later failure. The tool returns nonzero on failures. The backup is not promoted until `pg_dump`, archive listing, encryption, authenticated decryption, and a second archive listing succeed. GCM authentication rejects wrong keys, tampering, and truncation. To verify the artifact without restoring, copy it to a protected temporary location, decrypt with the recovery key using the library's validated restore path, and run `pg_restore --list` on the plaintext archive. Do not leave decrypted test files in an unprotected directory.

Retention cleanup runs after a successful backup and removes encrypted artifacts older than `HMS_BACKUP_RETENTION_DAYS`. Keep an off-host or immutable copy under the same encryption/key-recovery controls; retention in one directory does not protect against host loss or ransomware.

## Restore procedure

Restore only with explicit operator authorization. The target database must already exist, be disposable or explicitly approved, and be empty. The command never drops objects or uses `--clean`; a non-empty target normally fails safely. First provision PostgreSQL tools, connectivity, the matching historical key, and a restricted work directory. Confirm target and backup identity with a second operator according to local policy.

```powershell
$env:HMS_BACKUP_DIR = 'D:\HMS\Backups'
$env:HMS_BACKUP_KEY = '<retrieve the matching 64-character key from the secret manager>'
& 'C:\Program Files\HMS\hms_backup.exe' restore `
  'D:\HMS\Backups\hms-YYYYMMDDTHHMMSSZ-id.hmsbak' `
  'hms_recovery_test' --confirm-target 'hms_recovery_test'
if ($LASTEXITCODE -ne 0) { throw 'Restore failed; inspect sanitized tool output and PostgreSQL server logs.' }
```

The target name is restricted to a simple PostgreSQL identifier and must exactly match `--confirm-target`. The tool decrypts and authenticates the file, lists the archive, requires the target's `public` schema to contain no tables, views, sequences, or foreign tables, then restores with `--single-transaction --exit-on-error --no-owner --no-privileges` and checks PostgreSQL connectivity. It records duration in `restore-status.txt`. The PostgreSQL tools may emit diagnostics; do not capture or publish secret-bearing environment dumps. Validate application-specific schema and representative records with the deployment's approved SQL checks before directing traffic to the recovered system.

For production recovery, an authorized DBA must provision the target, verify the selected artifact and key version, run the same explicit restore command against the approved target, validate schema/constraints/sequences and application health, and obtain service-owner approval before traffic cutover. There is no automatic production restore or database drop in this utility.

## RPO, RTO, and verification limits

The recovery timer starts when the incident is declared and recovery is authorized; it ends only when the restored database passes data and application checks and is ready for service. The four-hour RTO includes infrastructure provisioning, key retrieval, archive transfer, decryption, restore, validation, and cutover. `restore-status.txt` measures only the local decrypt/restore/connectivity command duration, not the full incident-to-service interval.

Automated tests cover AES-256-GCM round trip, randomized nonces, confidentiality, wrong keys, tampering, truncation, and due detection. They do not run `pg_dump` or restore a live PostgreSQL database in ordinary CI. No production-scale recovery drill has been demonstrated by this implementation. The four-hour RTO remains a deployment objective until a timed drill uses representative production data volume and infrastructure, validates restored application data, records all elapsed phases, and completes within four hours. Monitor task freshness so a missed run alerts before the 24-hour backup objective is exceeded.
