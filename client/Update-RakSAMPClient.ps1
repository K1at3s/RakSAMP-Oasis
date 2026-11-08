[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'High')]
param(
    [Parameter(Mandatory = $true)][ValidateSet('UIF', 'Oasis')][string]$Project,
    [Parameter(Mandatory = $true)][string]$InstallRoot,
    [string]$Tag = ''
)
$ErrorActionPreference = 'Stop'

$repo = if ($Project -eq 'UIF') { 'K1at3s/RakSAMP' } else { 'K1at3s/RakSAMP-Oasis' }
$expectedRemote = "https://github.com/$repo"
$root = [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $InstallRoot).Path)
$bin = Join-Path $root 'client/bin'
$targetExe = Join-Path $bin 'RakSAMPClient.exe'
if (-not (Test-Path -LiteralPath $targetExe -PathType Leaf)) {
    throw "Client executable not found: $targetExe. InstallRoot must be this project's root folder."
}

$git = Get-Command git -ErrorAction SilentlyContinue
if ($git -and (Test-Path -LiteralPath (Join-Path $root '.git'))) {
    $remote = (& git -c "safe.directory=$root" -C $root remote get-url origin 2>$null | Select-Object -First 1)
    if ($LASTEXITCODE -eq 0 -and $remote) {
        $normalizedRemote = $remote.Trim() -replace '^git@github\.com:', 'https://github.com/' -replace '^ssh://git@github\.com/', 'https://github.com/' -replace '\.git/?$', ''
        if ($normalizedRemote -ine $expectedRemote) {
            throw "Project/repository mismatch. Selected $Project does not match this folder's Git remote. No files were changed."
        }
    }
}

$running = @(Get-Process -Name 'RakSAMPClient' -ErrorAction SilentlyContinue)
if ($running.Count -gt 0) { throw 'Close RakSAMPClient before updating. The updater will not stop it for you.' }
$gh = Get-Command gh -ErrorAction SilentlyContinue
if (-not $gh) { throw 'GitHub CLI (gh) is required. Install it and authenticate with gh auth login first.' }

$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$stage = [IO.Path]::GetFullPath((Join-Path $tempRoot ("RakSAMP-update-$([Guid]::NewGuid().ToString('N'))")))
if (-not $stage.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Temporary download path is outside the system temp folder.' }
[IO.Directory]::CreateDirectory($stage) | Out-Null
try {
    $downloadArgs = @('release', 'download', '--repo', $repo, '--pattern', 'RakSAMPClient.exe', '--pattern', 'SHA256SUMS.txt', '--dir', $stage)
    if ($Tag) { $downloadArgs += $Tag }
    & $gh.Source @downloadArgs
    if ($LASTEXITCODE -ne 0) { throw "Could not download a release from $repo. Check GitHub access and the selected tag." }

    $releaseExe = Join-Path $stage 'RakSAMPClient.exe'
    $manifestPath = Join-Path $stage 'SHA256SUMS.txt'
    if (-not (Test-Path -LiteralPath $releaseExe -PathType Leaf) -or -not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw 'The selected release is missing the client executable or checksum manifest.'
    }
    $manifest = [IO.File]::ReadAllText($manifestPath)
    $matches = [regex]::Matches($manifest, '(?im)^([a-f0-9]{64})\s+\*?RakSAMPClient\.exe\s*$')
    if ($matches.Count -ne 1) { throw 'SHA256SUMS.txt must contain exactly one RakSAMPClient.exe hash.' }
    $expectedHash = $matches[0].Groups[1].Value.ToLowerInvariant()
    $actualHash = (Get-FileHash -LiteralPath $releaseExe -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -ne $expectedHash) { throw 'Release EXE checksum mismatch. Nothing was installed.' }

    $version = (Get-Item -LiteralPath $releaseExe).VersionInfo.FileVersion
    if (-not $version) { $version = 'version metadata unavailable' }
    Write-Output "Project: $Project ($repo)"
    Write-Output "Release: $(if ($Tag) { $Tag } else { 'latest' })"
    Write-Output "Client: $version"
    Write-Output "SHA-256: $actualHash"
    Write-Output "Install target: $targetExe"
    Write-Output 'Only RakSAMPClient.exe will be replaced. XML and runtime data will be left in place.'

    if ($PSCmdlet.ShouldProcess($targetExe, "Install verified $Project client")) {
        $backupDir = Join-Path $bin 'backups'
        [IO.Directory]::CreateDirectory($backupDir) | Out-Null
        $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
        $oldHash = (Get-FileHash -LiteralPath $targetExe -Algorithm SHA256).Hash.ToLowerInvariant()
        $backupExe = Join-Path $backupDir ("RakSAMPClient-$stamp-$($oldHash.Substring(0, 8)).exe")
        [IO.File]::Copy($targetExe, $backupExe, $false)
        $backupHash = (Get-FileHash -LiteralPath $backupExe -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($backupHash -ne $oldHash) { throw 'Could not verify the old EXE backup; the installed client was not changed.' }
        $candidateExe = Join-Path $bin (".RakSAMPClient-$([Guid]::NewGuid().ToString('N')).tmp")
        try {
            [IO.File]::Copy($releaseExe, $candidateExe, $false)
            $candidateHash = (Get-FileHash -LiteralPath $candidateExe -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($candidateHash -ne $expectedHash) { throw 'Staged executable checksum mismatch; the original client is still in place.' }
            $swapBackup = Join-Path $bin (".RakSAMPClient-$([Guid]::NewGuid().ToString('N')).swap")
            [IO.File]::Replace($candidateExe, $targetExe, $swapBackup)
            $installedHash = (Get-FileHash -LiteralPath $targetExe -Algorithm SHA256).Hash.ToLowerInvariant()
            $swappedHash = (Get-FileHash -LiteralPath $swapBackup -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($installedHash -ne $expectedHash -or $swappedHash -ne $oldHash) {
                $restoreExe = Join-Path $bin (".RakSAMPClient-$([Guid]::NewGuid().ToString('N')).restore")
                $restoreOld = $restoreExe + '.old'
                [IO.File]::Copy($backupExe, $restoreExe, $false)
                [IO.File]::Replace($restoreExe, $targetExe, $restoreOld)
                if (Test-Path -LiteralPath $restoreOld) { [IO.File]::Delete($restoreOld) }
                throw 'Post-install verification failed; the previous executable was restored.'
            }
            [IO.File]::Delete($swapBackup)
        } catch {
            throw "Update failed. The original remains backed up at '$backupExe'. Details: $($_.Exception.Message)"
        } finally {
            if (Test-Path -LiteralPath $candidateExe) { Remove-Item -LiteralPath $candidateExe -Force }
            if ($swapBackup -and (Test-Path -LiteralPath $swapBackup)) { [IO.File]::Delete($swapBackup) }
        }
        Write-Output "Update complete. Previous executable backup: $backupExe"
        Write-Output 'To roll back, close the client and copy that backup over client/bin/RakSAMPClient.exe.'
    }
} finally {
    foreach ($name in @('RakSAMPClient.exe', 'SHA256SUMS.txt')) {
        $path = Join-Path $stage $name
        if (Test-Path -LiteralPath $path) { [IO.File]::Delete($path) }
    }
    if (Test-Path -LiteralPath $stage) { [IO.Directory]::Delete($stage, $false) }
}
