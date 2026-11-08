param(
    [string]$MySqlBase = 'C:/Program Files/MySQL/MySQL Server 8.4',
    [string]$Runtime = 'bun',
    [string[]]$AdditionalBackendRoots = @()
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$work = Join-Path $PSScriptRoot ('obj/mysql-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$listener = New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback, 0)
$listener.Start()
$port = $listener.LocalEndpoint.Port
$listener.Stop()
$mysqld = Join-Path $MySqlBase 'bin/mysqld.exe'
$mysqladmin = Join-Path $MySqlBase 'bin/mysqladmin.exe'
& $mysqld --no-defaults --initialize-insecure "--basedir=$MySqlBase" "--datadir=$work/data" "--log-error=$work/initialize.log"
if ($LASTEXITCODE -ne 0) { throw "Test MySQL initialization failed; see $work/initialize.log" }
$arguments = @('--no-defaults', ('--basedir="' + $MySqlBase + '"'), ('--datadir="' + $work + '/data"'), '--bind-address=127.0.0.1', "--port=$port", '--mysqlx=OFF', '--skip-log-bin', '--innodb-buffer-pool-size=64M', ('--log-error="' + $work + '/server.log"'))
$server = Start-Process -FilePath $mysqld -ArgumentList $arguments -WindowStyle Hidden -PassThru
$previousUrl = $env:TEST_MYSQL_URL
try {
    $ready = $false
    for ($i = 0; $i -lt 60; $i++) {
        if ($server.HasExited) { throw "Test MySQL exited; see $work/server.log" }
        $socket = New-Object Net.Sockets.TcpClient
        try { $socket.Connect('127.0.0.1', $port); $ready = $true; break } catch { Start-Sleep -Milliseconds 500 } finally { $socket.Dispose() }
    }
    if (!$ready) { throw 'Isolated MySQL did not become ready' }
    $env:TEST_MYSQL_URL = "mysql://root@127.0.0.1:$port"
    foreach ($backend in (@($root) + $AdditionalBackendRoots)) {
        Push-Location $backend
        try { & $Runtime mysql.integration.js; if ($LASTEXITCODE -ne 0) { throw "MySQL integration failed: $backend" }; & (Join-Path $backend 'tests/backup-restore.integration.ps1') -Port $port -MySqlBin (Join-Path $MySqlBase 'bin'); if ($LASTEXITCODE -ne 0) { throw "Backup integration failed: $backend" } }
        finally { Pop-Location }
    }
} finally {
    $env:TEST_MYSQL_URL = $previousUrl
    if (!$server.HasExited) {
        & $mysqladmin --no-defaults --protocol=TCP --host=127.0.0.1 "--port=$port" --user=root shutdown
        if (!$server.WaitForExit(10000)) { Stop-Process -Id $server.Id }
    }
}
