param(
    [ValidateSet('Status','Apply','Restore')][string]$Action = 'Status',
    [string]$GameRoot = 'I:\game\xbox\SCDA-XBERecomp-2',
    [string]$Backup
)
$ErrorActionPreference = 'Stop'
$packRoot = $PSScriptRoot
$payload = Join-Path $packRoot 'staging\x01_Iceland_a.remaster.lin'
$manifest = Get-Content -LiteralPath (Join-Path $packRoot 'staging\manifest.json') -Raw | ConvertFrom-Json
$target = Join-Path $GameRoot 'assets\Default\DataXb\LMaps\x01_Iceland_a\x01_Iceland_a.lin'
$journal = Join-Path $packRoot 'installation-history.jsonl'
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function CheckClosed {
    $running = Get-Process -Name 'SCDA','SCDA_Versus' -ErrorAction SilentlyContinue
    if ($running) { throw 'Close SCDA and keep it closed before Apply or Restore.' }
}
if (!(Test-Path -LiteralPath $target -PathType Leaf)) { throw "LIN not found: $target" }
if ((Hash $payload) -ne $manifest.staged_sha256) { throw 'The pack hash does not match the manifest. Installation canceled.' }
$current = Hash $target
if ($Action -eq 'Status') {
    [pscustomobject]@{Target=$target; CurrentSHA256=$current; PackSHA256=$manifest.staged_sha256; PackInstalled=($current -eq $manifest.staged_sha256); Textures=$manifest.textures; Recreated=$manifest.methods.ai_reconstruction; Technical2x=$manifest.methods.technical_interpolation}
    return
}
CheckClosed
if ($Action -eq 'Apply' -and $current -eq $manifest.staged_sha256) { Write-Output 'This pack is already installed.'; return }
# Cached archives can use different framing/padding. Never overwrite them as if they were source LINs.
$cacheRoot = Join-Path $GameRoot 'out\build\release\hdd'
if (Test-Path -LiteralPath $cacheRoot) {
    $cached = @(Get-ChildItem -LiteralPath $cacheRoot -Recurse -File -Filter 'x01_iceland_a.lin')
    if ($cached.Count) { throw ('A cached copy of the LIN exists. Verify its format before installation: ' + ($cached.FullName -join ', ')) }
}
if ($Action -eq 'Restore') {
    if (!$Backup) {
        if (!(Test-Path -LiteralPath $journal)) { throw 'No installation is recorded. Use -Backup with an explicit backup.' }
        $last = Get-Content -LiteralPath $journal | ForEach-Object { $_ | ConvertFrom-Json } | Where-Object { $_.action -eq 'Apply' -and $_.target -eq $target } | Select-Object -Last 1
        if (!$last) { throw 'No Apply installation is recorded.' }
        $Backup = $last.backup
        if ((Hash $Backup) -ne $last.before) { throw 'The backup no longer matches the recorded hash.' }
    }
    if (!(Test-Path -LiteralPath $Backup -PathType Leaf)) { throw "Backup not found: $Backup" }
    $source = $Backup
} else { $source = $payload }
$desired = Hash $source
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$save = "$target.remaster-$($Action.ToLowerInvariant())-$stamp.save"
$temporary = "$target.remaster-$stamp.tmp"
# Check exclusive access before creating any backups or replacing the target.
$handle = [IO.File]::Open($target,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
$handle.Dispose()
Copy-Item -LiteralPath $target -Destination $save
if ((Hash $save) -ne $current) { throw 'Backup verification failed.' }
try {
    Copy-Item -LiteralPath $source -Destination $temporary
    if ((Hash $temporary) -ne $desired) { throw 'Temporary-copy verification failed.' }
    [IO.File]::Replace($temporary,$target,$save,$true)
    if ((Hash $target) -ne $desired) { throw 'Verification after replacement failed.' }
} finally {
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
}
[ordered]@{time=(Get-Date).ToString('o');action=$Action;target=$target;backup=$save;before=$current;after=$desired} | ConvertTo-Json -Compress | Add-Content -LiteralPath $journal -Encoding UTF8
Write-Output "$Action complete. Backup: $save"
