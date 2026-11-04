param(
    [ValidateSet('UIF', 'Oasis')][string]$Project = 'UIF',
    [string]$ClientRoot = '',
    [string]$OutputPath = ''
)
$ErrorActionPreference = 'Stop'
if ($ClientRoot) { $root = [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $ClientRoot).Path) }
else { $root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if (-not $OutputPath) {
    $desktop = [Environment]::GetFolderPath('DesktopDirectory')
    if (-not $desktop) { $desktop = [Environment]::GetFolderPath('MyDocuments') }
    if (-not $desktop) { $desktop = (Get-Location).Path }
    $OutputPath = Join-Path $desktop ("RakSAMP-$Project-diagnostic-$stamp.zip")
}
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) { [IO.Directory]::CreateDirectory($outputDirectory) | Out-Null }
$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$work = [IO.Path]::GetFullPath((Join-Path $tempRoot ("RakSAMP-diagnostic-$([Guid]::NewGuid().ToString('N'))")))
if (-not $work.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Temporary report path is outside the system temp folder.' }
[IO.Directory]::CreateDirectory($work) | Out-Null
try {
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add('RakSAMP privacy-filtered diagnostic report')
    $lines.Add("Project: $Project")
    $lines.Add("Created UTC: $([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))")
    $lines.Add("Windows: $([Environment]::OSVersion.VersionString)")
    $exe = Join-Path $root 'client/bin/RakSAMPClient.exe'
    if (Test-Path -LiteralPath $exe) {
        $version = (Get-Item -LiteralPath $exe).VersionInfo.FileVersion
        $lines.Add("Client file version: $(if($version){$version}else{'unknown'})")
    } else { $lines.Add('Client file version: unavailable') }
    $lines.Add('')
    $lines.Add('Only selected technical event lines are included. Chat, scan/textdraw logs, XML, backend config, dumps, and full paths are excluded.')
    $lines.Add('This tool does not upload or send the report. Review the report before sharing it.')
    $lines.Add('')
    $logDirs = @((Join-Path $root 'logs'), (Join-Path $root 'client/bin/logs'))
    $logs = foreach ($dir in $logDirs) {
        if (Test-Path -LiteralPath $dir) {
            Get-ChildItem -LiteralPath $dir -File -Filter 'RakSAMPClient-20??-??-??.log' -ErrorAction SilentlyContinue
        }
    }
    $latest = $logs | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if (-not $latest) {
        $lines.Add('No daily client log was found in the standard log folders.')
    } else {
        $lines.Add("Recent filtered events from the latest daily log (last write UTC: $($latest.LastWriteTimeUtc.ToString('yyyy-MM-ddTHH:mm:ssZ'))):")
        $selected = @(Get-Content -LiteralPath $latest.FullName -Encoding UTF8 -Tail 3000 | Where-Object { $_ -match '\[(BACKEND|OUTBOX|NET|LOGIN|SEC|UNLINK)\]' })
        foreach ($line in ($selected | Select-Object -Last 250)) {
            $safe = $line
            $safe = [regex]::Replace($safe, '(?i)(password|passwd|token|authorization|secret|hwid|account|username)\s*[:=]\s*[^\s,;]+', '$1=[REDACTED]')
            $safe = [regex]::Replace($safe, '(?i)([?&](?:name|player|group|account|hwid|id)=)[^&\s]+', '$1[REDACTED]')
            $safe = [regex]::Replace($safe, '(?i)(/login|/register|/changepass|\?changepass|/email|\?email|/rcon\s+login)\s+\S+', '$1 [REDACTED]')
            $safe = [regex]::Replace($safe, '(?i)(Connecting to )\S+', '$1[SERVER REDACTED]')
            $safe = [regex]::Replace($safe, '(?<![\w])(?:\d{1,3}\.){3}\d{1,3}(?::\d{1,5})?(?!\w)', '[ADDRESS REDACTED]')
            $safe = [regex]::Replace($safe, '(?i)\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}\b', '[EMAIL REDACTED]')
            $safe = [regex]::Replace($safe, '(?i)\b[a-f0-9]{32,64}\b', '[IDENTIFIER REDACTED]')
            $lines.Add($safe)
        }
        if ($selected.Count -eq 0) { $lines.Add('No selected technical event lines were found.') }
    }
    [IO.File]::WriteAllLines((Join-Path $work 'diagnostics.txt'), $lines, [Text.UTF8Encoding]::new($false))
    Compress-Archive -LiteralPath (Join-Path $work 'diagnostics.txt') -DestinationPath $OutputPath -CompressionLevel Optimal -Force
    Write-Output "Created local report: $OutputPath"
    Write-Output 'Review diagnostics.txt inside the archive before sharing it. Nothing was uploaded.'
} finally {
    if (Test-Path -LiteralPath $work) {
        $reportFile = Join-Path $work 'diagnostics.txt'
        if (Test-Path -LiteralPath $reportFile) { Remove-Item -LiteralPath $reportFile -Force }
        Remove-Item -LiteralPath $work -Force
    }
}
