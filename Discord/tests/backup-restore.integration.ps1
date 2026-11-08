param(
    [Parameter(Mandatory = $true)][ValidateRange(1, 65535)][int]$Port,
    [string]$MySqlBin = 'C:/Program Files/MySQL/MySQL Server 8.4/bin'
)
$ErrorActionPreference = 'Stop'
$mysqlExe = Join-Path $MySqlBin 'mysql.exe'
if (-not (Test-Path -LiteralPath $mysqlExe -PathType Leaf)) { throw 'mysql.exe was not found for backup integration.' }
$backendRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$backupScript = Join-Path $PSScriptRoot '../Backup-MySql.ps1'
$restoreScript = Join-Path $PSScriptRoot '../Restore-MySql.ps1'
$database = 'raksamp_br_test_' + [Guid]::NewGuid().ToString('N').Substring(0, 10)
$backupDir = Join-Path $PSScriptRoot ('obj/backup-restore-' + [Guid]::NewGuid().ToString('N'))
$hadPassword = Test-Path Env:RAKSAMP_DB_PASSWORD
$previousPassword = $env:RAKSAMP_DB_PASSWORD
$env:RAKSAMP_DB_PASSWORD = ''
try {
    & $mysqlExe --no-defaults --protocol=TCP --host=127.0.0.1 "--port=$Port" --user=root --execute "CREATE DATABASE $database CHARACTER SET utf8mb4; CREATE TABLE $database.restore_check (id INT PRIMARY KEY, marker VARCHAR(32)); INSERT INTO $database.restore_check VALUES (1,'original');"
    if ($LASTEXITCODE -ne 0) { throw 'Could not create the isolated backup test database.' }
    $backupOutput = @(& $backupScript -Database $database -HostName 127.0.0.1 -Port $Port -User root -MySqlBin $MySqlBin -BackupDirectory $backupDir -RetentionDays 1)
    $backupLine = $backupOutput | Where-Object { $_ -like 'Backup created:*' } | Select-Object -First 1
    if (-not $backupLine) { throw 'Backup script did not report a dump path.' }
    $backupPath = $backupLine.Substring('Backup created: '.Length)
    & $mysqlExe --no-defaults --protocol=TCP --host=127.0.0.1 "--port=$Port" --user=root --database=$database --execute "UPDATE restore_check SET marker='modified' WHERE id=1;"
    if ($LASTEXITCODE -ne 0) { throw 'Could not mutate the isolated test database before restore.' }
    & $restoreScript -Database $database -RestorePath $backupPath -ConfirmDatabase $database -HostName 127.0.0.1 -Port $Port -User root -MySqlBin $MySqlBin -BackupDirectory $backupDir -RetentionDays 1 -Confirm:$false
    if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) { throw 'Restore script failed.' }
    $result = & $mysqlExe --no-defaults --protocol=TCP --host=127.0.0.1 "--port=$Port" --user=root --database=$database --batch --skip-column-names --execute 'SELECT marker FROM restore_check WHERE id=1;'
    if ($LASTEXITCODE -ne 0 -or ($result | Out-String).Trim() -ne 'original') { throw 'Restored database content did not match the original backup.' }
    if (-not (Test-Path -LiteralPath ($backupPath + '.sha256'))) { throw 'Backup checksum sidecar is missing.' }
    $dumps = @(Get-ChildItem -LiteralPath $backupDir -Filter "$database-*.sql" -File)
    if ($dumps.Count -lt 2) { throw 'Restore did not create a pre-restore safety backup.' }
    Write-Output "MySQL backup/restore integration passed for isolated schema $database."
} finally {
    & $mysqlExe --no-defaults --protocol=TCP --host=127.0.0.1 "--port=$Port" --user=root --execute "DROP DATABASE IF EXISTS $database;" | Out-Null
    if ($hadPassword) { $env:RAKSAMP_DB_PASSWORD = $previousPassword } else { Remove-Item Env:RAKSAMP_DB_PASSWORD -ErrorAction SilentlyContinue }
    if (Test-Path -LiteralPath $backupDir) {
        $full = [IO.Path]::GetFullPath($backupDir)
        $objRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'obj'))
        if ($full.StartsWith($objRoot, [StringComparison]::OrdinalIgnoreCase) -and (Split-Path $full -Leaf).StartsWith('backup-restore-')) { Remove-Item -LiteralPath $full -Recurse -Force }
    }
}
