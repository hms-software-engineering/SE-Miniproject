# PostgreSQL encrypted backup on Windows

`Invoke-HmsEncryptedPostgresBackup.ps1` runs `pg_dump`, encrypts the archive with Windows DPAPI, and publishes it only after the dump and encryption complete. It writes sanitized job results to `hms-backup-job.log`; a failed run returns a nonzero exit and records `BackupPublished=false`. Restore decrypts and validates the archive before invoking `pg_restore` against a separate database. It never uses `pg_restore --clean`.

## Provisioning requirements

Before scheduling a backup, an operator must supply the real HMS PostgreSQL host, port, database, role, PostgreSQL data directory, a backup directory outside the database data directory (preferably on independent storage), and an external PostgreSQL password file if password authentication is used. Do not put connection passwords, TLS private keys, or database files in the repository. The script checks the optional `PGPASSFILE` ACL and logs no password or native-tool output.

The archive uses DPAPI `CurrentUser` scope for its encrypted manifest and data chunks. Restore therefore requires the same Windows account and DPAPI profile that created the backup. This implementation does not provide cross-account or cross-machine key recovery; provision and test a separate recovery/key-escrow design before relying on it for disaster recovery.

The task installer registers a daily 02:00 local-time task for the current interactive user. The account must be logged in at run time for this principal type. It does not replace an existing task. Operators must inspect Task Scheduler history and the backup log, alert on nonzero runs, and verify that each archive is copied to approved independent storage. A daily schedule by itself is not proof that the RPO was met.

## Registering after deployment configuration is available

Run PowerShell as the intended backup account. Keep the password file and backup directory outside the repository. Supply the actual values for the database and data-directory paths; do not use the synthetic Stage 6 values.

```powershell
$register = @{
    BackupScriptPath = (Resolve-Path .\PostgreSQL\backup\Invoke-HmsEncryptedPostgresBackup.ps1).Path
    PgBinPath = 'C:\Program Files\PostgreSQL\17\bin'
    DbHost = '127.0.0.1'
    DbPort = 5432
    Database = '<configured HMS database>'
    Username = '<least-privilege backup role>'
    BackupDirectory = '<separate approved backup storage>'
    DatabaseDataDirectory = '<configured PostgreSQL data directory>'
    PgPassFile = '<external protected pgpass file>'
}
& .\PostgreSQL\backup\Register-HmsDailyBackupTask.ps1 @register
```

After registration, confirm the task principal, trigger, last result, and log. Then restore one archive to a newly created, empty disposable PostgreSQL database using the same Windows account and DPAPI profile. Compare application-level row counts/checksums and measure the restore against the four-hour RTO before marking the operational backup and recovery procedure verified.

## Stage 6 verification and limits

An isolated PostgreSQL 17.11 drill verified encrypted publication, absence of a synthetic plaintext marker, successful restore with a matching row fingerprint, and failure/no-publication behavior for a missing source database. The Stage 6 installer was not registered because no HMS staging database configuration or approved backup storage was available. The official daily backup job and recovery procedure remain unverified. See [Stage 6 backup evidence](../../evidence/automated_tests/stage6_20261115/backup_restore_execution.txt).
