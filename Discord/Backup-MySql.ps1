[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[A-Za-z0-9_]{1,64}$')][string]$Database,
    [string]$HostName = '127.0.0.1',
    [ValidateRange(1, 65535)][int]$Port = 3306,
    [ValidatePattern('^[A-Za-z0-9_.@-]{1,128}$')][string]$User = 'raksamp',
    [ValidatePattern('^[A-Za-z0-9_.-]{1,64}$')][string]$LoginPath = '',
    [string]$MySqlBin = 'C:/Program Files/MySQL/MySQL Server 8.4/bin',
    [string]$BackupDirectory = (Join-Path $PSScriptRoot 'backups'),
    [ValidateRange(1, 3650)][int]$RetentionDays = 14
)
$ErrorActionPreference = 'Stop'
if ($HostName -notmatch '^[A-Za-z0-9._:-]{1,255}$') { throw 'Invalid MySQL host.' }
$dumpExe = Join-Path $MySqlBin 'mysqldump.exe'
if (-not (Test-Path -LiteralPath $dumpExe -PathType Leaf)) {
    $found = Get-Command mysqldump -ErrorAction SilentlyContinue
    if ($found) { $dumpExe = $found.Source } else { throw 'mysqldump.exe was not found. Set -MySqlBin to the MySQL bin folder.' }
}
$hasPassword = Test-Path Env:RAKSAMP_DB_PASSWORD
$password = $env:RAKSAMP_DB_PASSWORD
$passwordBuffer = $null
if (-not $LoginPath -and -not $hasPassword) {
    $secure = Read-Host "MySQL password for $User@$HostName/$Database" -AsSecureString
    $passwordBuffer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    $password = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($passwordBuffer)
}
[IO.Directory]::CreateDirectory($BackupDirectory) | Out-Null
$backupRoot = [IO.Path]::GetFullPath($BackupDirectory)
$stamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ', [Globalization.CultureInfo]::InvariantCulture)
$backupPath = Join-Path $backupRoot ("$Database-$stamp.sql")
$partialPath = $backupPath + '.partial'
$manifestPath = $backupPath + '.sha256'
$arguments = @()
if ($LoginPath) { $arguments += "--login-path=$LoginPath" }
$arguments += @("--host=$HostName", "--port=$Port", "--user=$User", '--single-transaction', '--quick', '--routines', '--events', '--triggers', '--add-drop-table', '--no-tablespaces', '--set-gtid-purged=OFF', '--databases', $Database)
$process = $null
$file = $null
try {
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $dumpExe
    $start.Arguments = ($arguments -join ' ')
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    if ($LoginPath) { [void]$start.EnvironmentVariables.Remove('MYSQL_PWD') }
    else { $start.EnvironmentVariables['MYSQL_PWD'] = $password }
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $start
    $file = New-Object IO.FileStream($partialPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    if (-not $process.Start()) { throw 'Could not start mysqldump.' }
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $process.StandardOutput.BaseStream.CopyTo($file)
    $file.Flush()
    $file.Dispose(); $file = $null
    $process.WaitForExit()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    if ($process.ExitCode -ne 0) { throw "mysqldump failed with exit code $($process.ExitCode). Check the host, account privileges, and MySQL logs." }
    if ((Get-Item -LiteralPath $partialPath).Length -lt 32) { throw 'mysqldump returned an empty or incomplete backup.' }
    [IO.File]::Move($partialPath, $backupPath)
    $hash = (Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText($manifestPath, "$hash  $(Split-Path $backupPath -Leaf)`n", [Text.UTF8Encoding]::new($false))
    $cutoff = [DateTime]::UtcNow.AddDays(-$RetentionDays)
    foreach ($old in (Get-ChildItem -LiteralPath $backupRoot -File -Filter "$Database-*.sql")) {
        if ($old.LastWriteTimeUtc -lt $cutoff) {
            [IO.File]::Delete($old.FullName)
            $oldManifest = $old.FullName + '.sha256'
            if (Test-Path -LiteralPath $oldManifest) { [IO.File]::Delete($oldManifest) }
        }
    }
    Write-Output "Backup created: $backupPath"
    Write-Output "SHA-256: $hash"
} finally {
    if ($file) { $file.Dispose() }
    if ($process) { if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }; $process.Dispose() }
    if (Test-Path -LiteralPath $partialPath) { [IO.File]::Delete($partialPath) }
    if ($passwordBuffer -ne $null) { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($passwordBuffer) }
    $password = $null
}
