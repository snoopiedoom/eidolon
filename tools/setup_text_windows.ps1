param(
    [string]$CacheRoot = "$PSScriptRoot/../.cache/sdl_ttf"
)

$ErrorActionPreference = "Stop"
$version = "3.2.2"
$sha256 = "67805c5babfc49ca0c56882dc9b8cabbcdd1e6f9edde10ddac91ddb38f3afb8c"
$cache = [System.IO.Path]::GetFullPath($CacheRoot)
$archive = Join-Path $cache "SDL3_ttf-devel-$version-VC.zip"
$runtime = Join-Path $cache "SDL3_ttf-$version"
New-Item -ItemType Directory -Force $cache | Out-Null

function Get-Sha256([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $hash = [System.Security.Cryptography.SHA256]::Create().ComputeHash($stream)
        return ([System.BitConverter]::ToString($hash)).Replace("-", "").ToLowerInvariant()
    } finally {
        $stream.Dispose()
    }
}

if (-not (Test-Path -LiteralPath $archive) -or (Get-Sha256 $archive) -ne $sha256) {
    Write-Host "downloading SDL_ttf $version"
    Invoke-WebRequest `
        "https://github.com/libsdl-org/SDL_ttf/releases/download/release-$version/SDL3_ttf-devel-$version-VC.zip" `
        -OutFile $archive
}
$actual = Get-Sha256 $archive
if ($actual -ne $sha256) {
    throw "checksum mismatch for $archive`nexpected $sha256`nactual   $actual"
}
if (-not (Test-Path -LiteralPath (Join-Path $runtime "include/SDL3_ttf/SDL_ttf.h"))) {
    Expand-Archive -LiteralPath $archive -DestinationPath $cache -Force
}
Write-Host "SDL_ttf runtime ready: $runtime"
