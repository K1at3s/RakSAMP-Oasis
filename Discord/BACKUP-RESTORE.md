# MySQL backup and restore

The backup scripts dump only the selected database and save files under `Discord/backups/`, which is ignored by Git. Dumps contain license, HWID, account, and group data; keep this folder on a protected disk and copy it to a separate protected location. The scripts do not upload backups.

The default MySQL client location is `C:/Program Files/MySQL/MySQL Server 8.4/bin`. Pass `-MySqlBin` if yours is elsewhere. If `RAKSAMP_DB_PASSWORD` is not set, the scripts prompt for the password as a secure input; they pass it to the MySQL process through its environment, never as a command-line argument.

## Create a backup

From `Discord/`:

```powershell
./Backup-MySql.ps1 -Database raksamp_oasis
```

For Oasis, use `-Database raksamp_oasis_oasis`. A backup is a timestamped SQL dump plus a SHA-256 sidecar. The script uses a consistent InnoDB snapshot, includes routines, triggers and events, and removes matching backups older than 14 days. Override that with `-RetentionDays` or choose another folder with `-BackupDirectory`.

For a scheduled task that must run without an interactive password prompt, create a MySQL login path once. MySQL stores it in the current Windows user profile:

```powershell
& 'C:/Program Files/MySQL/MySQL Server 8.4/bin/mysql_config_editor.exe' set --login-path=raksamp-backup --host=127.0.0.1 --user=raksamp --password
```

The tool prompts for the password; do not add it to the command. Then create a Windows Task Scheduler task under the same Windows account, set the action's program to `powershell.exe`, and use arguments similar to:

```text
-NoProfile -ExecutionPolicy Bypass -File "C:\path\to\RakSAMP\Discord\Backup-MySql.ps1" -Database raksamp_oasis -LoginPath raksamp-backup -RetentionDays 14
```

Create a separate login path and task for Oasis, using database `raksamp_oasis` and a distinct path name. The task must run as the same Windows user that created the login path.

## Restore a backup

Restore verifies the SQL file against its sidecar and requires the database name twice. It also creates a fresh pre-restore backup before changing an existing database:

```powershell
./Restore-MySql.ps1 -Database raksamp_oasis -ConfirmDatabase raksamp -RestorePath 'C:\protected-backups\raksamp_oasis-20261108T030000000Z.sql'
```

Use `raksamp_oasis` for Oasis. The script asks for confirmation before work begins. `-WhatIf` validates the backup and shows the target without restoring it. If the target database does not exist yet, `-SkipPreRestoreBackup` is required because there is no current database to snapshot; use it only after confirming the target, dump, and checksum.

If a restore fails, the pre-restore dump remains in `Discord/backups/`. After a successful restore, verify the backend can connect and check expected records before resuming the client.
