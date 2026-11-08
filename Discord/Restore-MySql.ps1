[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'High')]
param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[A-Za-z0-9_]{1,64}$')][string]$Database,
    [Parameter(Mandatory = $true)][string]$RestorePath,
    [Parameter(Mandatory = $true)][string]$ConfirmDatabase,
    [switch]$SkipPreRestoreBackup,
    [string]$HostName = '127.0.0.1',
    [ValidateRange(1, 65535)][int]$Port = 3306,
    [ValidatePattern('^[A-Za-z0-9_.@-]{1,128}$')][string]$User = 'raksamp',
    [ValidatePattern('^[A-Za-z0-9_.-]{1,64}$')][string]$LoginPath = '',
    [string]$MySqlBin = 'C:/Program Files/MySQL/MySQL Server 8.4/bin',
    [string]$BackupDirectory = (Join-Path $PSScriptRoot 'backups'),
    [ValidateRange(1, 3650)][int]$RetentionDays = 14
)
$ErrorActionPreference = 'Stop'
if ($ConfirmDatabase -cne $Database) { throw 'Database confirmation did not match; no data was changed.' }
if ($HostName -notmatch '^[A-Za-z0-9._:-]{1,255}$') { throw 'Invalid MySQL host.' }
$restoreFile = (Resolve-Path -LiteralPath $RestorePath).Path
if ([IO.Path]::GetExtension($restoreFile) -ine '.sql' -or (Get-Item -LiteralPath $restoreFile).Length -lt 32) { throw 'RestorePath must point to a non-empty .sql backup.' }
if ((Split-Path $restoreFile -Leaf) -notlike "$Database-*.sql") { throw 'Backup filename does not belong to the selected database.' }
$manifestPath = $restoreFile + '.sha256'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw 'Matching .sha256 sidecar is missing; refusing to restore an unverified dump.' }
$manifest = [IO.File]::ReadAllText($manifestPath)
$expectedLine = [regex]::Match($manifest, '(?im)^([a-f0-9]{64})\s+(.+\.sql)\s*$')
if (-not $expectedLine.Success -or $expectedLine.Groups[2].Value -cne (Split-Path $restoreFile -Leaf)) { throw 'Checksum manifest does not match this SQL filename.' }
$expectedHash = $expectedLine.Groups[1].Value.ToLowerInvariant()
$actualHash = (Get-FileHash -LiteralPath $restoreFile -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualHash -ne $expectedHash) { throw 'Backup checksum mismatch; no data was changed.' }
$mysqlExe = Join-Path $MySqlBin 'mysql.exe'
if (-not (Test-Path -LiteralPath $mysqlExe -PathType Leaf)) {
    $found = Get-Command mysql -ErrorAction SilentlyContinue
    if ($found) { $mysqlExe = $found.Source } else { throw 'mysql.exe was not found. Set -MySqlBin to the MySQL bin folder.' }
}
$hasPassword = Test-Path Env:RAKSAMP_DB_PASSWORD
$password = $env:RAKSAMP_DB_PASSWORD
$passwordBuffer = $null
try {
if ($PSCmdlet.ShouldProcess("$HostName`:$Port/$Database", "Restore verified SQL backup $(Split-Path $restoreFile -Leaf)")) {
    if (-not $LoginPath -and -not $hasPassword) {
        $secure = Read-Host "MySQL password for $User@$HostName/$Database" -AsSecureString
        $passwordBuffer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
        $password = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($passwordBuffer)
    }
    $hadPassword = Test-Path Env:RAKSAMP_DB_PASSWORD
    $previousPassword = $env:RAKSAMP_DB_PASSWORD
    try {
        $preRestoreOutput = @()
        if (-not $SkipPreRestoreBackup) {
            if (-not $LoginPath) { $env:RAKSAMP_DB_PASSWORD = $password }
            $backupScript = Join-Path $PSScriptRoot 'Backup-MySql.ps1'
            $preRestoreArgs = @{ Database = $Database; HostName = $HostName; Port = $Port; User = $User; MySqlBin = $MySqlBin; BackupDirectory = $BackupDirectory; RetentionDays = $RetentionDays }
            if ($LoginPath) { $preRestoreArgs.LoginPath = $LoginPath }
            $preRestoreOutput = @(& $backupScript @preRestoreArgs)
            if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) { throw 'Could not create the pre-restore safety backup.' }
        }
    } finally {
        if (-not $LoginPath) { if ($hadPassword) { $env:RAKSAMP_DB_PASSWORD = $previousPassword } else { Remove-Item Env:RAKSAMP_DB_PASSWORD -ErrorAction SilentlyContinue } }
    }

    $process = $null
    $inputFile = $null
    try {
        $start = New-Object Diagnostics.ProcessStartInfo
        $start.FileName = $mysqlExe
        $mysqlArgs = @()
        if ($LoginPath) { $mysqlArgs += "--login-path=$LoginPath" }
        $mysqlArgs += @("--host=$HostName", "--port=$Port", "--user=$User", '--default-character-set=utf8mb4')
        $start.Arguments = $mysqlArgs -join ' '
        $start.UseShellExecute = $false
        $start.CreateNoWindow = $true
        $start.RedirectStandardInput = $true
        $start.RedirectStandardError = $true
        if ($LoginPath) { [void]$start.EnvironmentVariables.Remove('MYSQL_PWD') }
        else { $start.EnvironmentVariables['MYSQL_PWD'] = $password }
        $process = New-Object Diagnostics.Process
        $process.StartInfo = $start
        if (-not $process.Start()) { throw 'Could not start mysql restore process.' }
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $inputFile = [IO.File]::OpenRead($restoreFile)
        $inputFile.CopyTo($process.StandardInput.BaseStream)
        $inputFile.Dispose(); $inputFile = $null
        $process.StandardInput.Close()
        $process.WaitForExit()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) {
            $preRestoreInfo = $preRestoreOutput | Where-Object { $_ -like 'Backup created:*' } | Select-Object -First 1
            if (-not $preRestoreInfo) { $preRestoreInfo = 'No pre-restore backup was requested.' }
            throw "mysql restore failed with exit code $($process.ExitCode). $preRestoreInfo"
        }
        Write-Output "Restore completed for $Database from $restoreFile"
        $preRestoreOutput | Where-Object { $_ -like 'Backup created:*' }
    } finally {
        if ($inputFile) { $inputFile.Dispose() }
        if ($process) { if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }; $process.Dispose() }
    }
}
} finally {
    if ($passwordBuffer -ne $null) { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($passwordBuffer) }
    $password = $null
}
