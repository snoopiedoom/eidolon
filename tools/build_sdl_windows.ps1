param(
    [string]$BuildRoot = "$PSScriptRoot/../.cache/sdl",
    [string]$CCompiler = "clang",
    [string]$CxxCompiler = "clang++",
    [string]$Generator = "Ninja",
    [string]$CompilerTarget = "",
    [string]$CompilerToolchain = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$repository = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$build = [System.IO.Path]::GetFullPath($BuildRoot)
$install = Join-Path $build "install"
$sdlSource = Join-Path $repository "lib/SDL"
$ttfSource = Join-Path $repository "lib/SDL_ttf"
$sdlBuild = Join-Path $build "build-sdl"
$ttfBuild = Join-Path $build "build-sdl-ttf"

if ($Clean) {
    $cacheRoot = [System.IO.Path]::GetFullPath((Join-Path $repository ".cache"))
    $cachePrefix = $cacheRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar) +
        [System.IO.Path]::DirectorySeparatorChar
    if (-not $build.StartsWith($cachePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean SDL build root outside the repository cache: $build"
    }
    if (Test-Path -LiteralPath $build) {
        Remove-Item -LiteralPath $build -Recurse -Force
    }
    Write-Host "removed SDL dependency build: $build"
    exit 0
}

function Assert-File([string]$Path, [string]$Hint) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing dependency source: $Path`n$Hint"
    }
}

function Invoke-CMake([string[]]$Arguments) {
    & cmake @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "cmake failed with exit code $LASTEXITCODE"
    }
}

Get-Command cmake -ErrorAction Stop | Out-Null
if ($Generator -eq "Ninja") {
    Get-Command ninja -ErrorAction Stop | Out-Null
}
Get-Command $CCompiler -ErrorAction Stop | Out-Null
Get-Command $CxxCompiler -ErrorAction Stop | Out-Null

$submoduleHint = "Run: git submodule update --init --recursive"
Assert-File (Join-Path $sdlSource "CMakeLists.txt") $submoduleHint
Assert-File (Join-Path $ttfSource "CMakeLists.txt") $submoduleHint
Assert-File (Join-Path $ttfSource "external/freetype/CMakeLists.txt") $submoduleHint
Assert-File (Join-Path $ttfSource "external/harfbuzz/CMakeLists.txt") $submoduleHint
Assert-File (Join-Path $ttfSource "external/plutosvg/CMakeLists.txt") $submoduleHint
Assert-File (Join-Path $ttfSource "external/plutovg/CMakeLists.txt") $submoduleHint

New-Item -ItemType Directory -Force -Path $build, $install | Out-Null

$compilerArguments = @(
    "-DCMAKE_C_COMPILER=$CCompiler",
    "-DCMAKE_CXX_COMPILER=$CxxCompiler"
)
if ($CompilerTarget) {
    $compilerArguments += "-DCMAKE_C_COMPILER_TARGET=$CompilerTarget"
    $compilerArguments += "-DCMAKE_CXX_COMPILER_TARGET=$CompilerTarget"
}
if ($CompilerToolchain) {
    $compilerArguments += "-DCMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN=$CompilerToolchain"
    $compilerArguments += "-DCMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN=$CompilerToolchain"
}

$commonArguments = @(
    "-G", $Generator,
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_INSTALL_PREFIX=$install"
) + $compilerArguments

Write-Host "configuring SDL 3.4.12"
Invoke-CMake (@(
    "-S", $sdlSource,
    "-B", $sdlBuild
) + $commonArguments + @(
    "-DSDL_SHARED=ON",
    "-DSDL_STATIC=OFF",
    "-DSDL_TESTS=OFF",
    "-DSDL_EXAMPLES=OFF",
    "-DSDL_INSTALL=ON",
    "-DSDL_UNINSTALL=OFF"
))
Invoke-CMake @("--build", $sdlBuild, "--target", "install", "--config", "Release")

Write-Host "configuring SDL_ttf 3.2.2"
Invoke-CMake (@(
    "-S", $ttfSource,
    "-B", $ttfBuild
) + $commonArguments + @(
    "-DCMAKE_PREFIX_PATH=$install",
    "-DBUILD_SHARED_LIBS=ON",
    "-DSDLTTF_INSTALL=ON",
    "-DSDLTTF_SAMPLES=OFF",
    "-DSDLTTF_VENDORED=ON",
    "-DSDLTTF_HARFBUZZ=ON",
    "-DSDLTTF_PLUTOSVG=ON"
))
Invoke-CMake @("--build", $ttfBuild, "--target", "install", "--config", "Release")

Assert-File (Join-Path $install "include/SDL3/SDL.h") "SDL installation did not produce headers."
Assert-File (Join-Path $install "include/SDL3_ttf/SDL_ttf.h") "SDL_ttf installation did not produce headers."
Assert-File (Join-Path $install "bin/SDL3.dll") "SDL installation did not produce SDL3.dll."
Assert-File (Join-Path $install "bin/SDL3_ttf.dll") "SDL_ttf installation did not produce SDL3_ttf.dll."

Write-Host "SDL dependency runtime ready: $install"
