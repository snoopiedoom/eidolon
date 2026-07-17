param(
    [string]$CacheRoot = "$PSScriptRoot/../.cache/affect"
)

$ErrorActionPreference = "Stop"
$ortVersion = "1.24.3"
$ortSha256 = "4fbfb85d0e9de9bb6fb8a9866a7cb477cbad404d889b236931bf3f5d547e5f48"
$modelCommit = "c4e1cea7f2827bc2db2f6a7b8ea4a35f28f3868d"
$modelSha256 = "0c1981c5b479674747911c8e2228f0c4ec90bf47bf66e830f7d4fc62be082958"

$cache = [System.IO.Path]::GetFullPath($CacheRoot)
$downloads = Join-Path $cache "downloads"
$runtime = Join-Path $cache "onnxruntime"
$model = Join-Path $cache "model"
New-Item -ItemType Directory -Force $downloads, $runtime, $model | Out-Null

function Get-VerifiedFile {
    param([string]$Uri, [string]$Path, [string]$Sha256)
    function Get-Sha256([string]$FilePath) {
        $stream = [System.IO.File]::OpenRead($FilePath)
        try {
            $hash = [System.Security.Cryptography.SHA256]::Create().ComputeHash($stream)
            return ([System.BitConverter]::ToString($hash)).Replace("-", "").ToLowerInvariant()
        } finally {
            $stream.Dispose()
        }
    }
    if (-not (Test-Path -LiteralPath $Path) -or
        (Get-Sha256 $Path) -ne $Sha256) {
        Write-Host "downloading $([System.IO.Path]::GetFileName($Path))"
        Invoke-WebRequest -Uri $Uri -OutFile $Path
    }
    $actual = Get-Sha256 $Path
    if ($actual -ne $Sha256) {
        throw "checksum mismatch for $Path`nexpected $Sha256`nactual   $actual"
    }
}

$ortZip = Join-Path $downloads "onnxruntime-win-x64-$ortVersion.zip"
Get-VerifiedFile `
    "https://github.com/microsoft/onnxruntime/releases/download/v$ortVersion/onnxruntime-win-x64-$ortVersion.zip" `
    $ortZip $ortSha256

$ortExtracted = Join-Path $downloads "onnxruntime-win-x64-$ortVersion"
if (-not (Test-Path -LiteralPath (Join-Path $ortExtracted "include/onnxruntime_c_api.h"))) {
    Expand-Archive -LiteralPath $ortZip -DestinationPath $downloads -Force
}
Copy-Item -Recurse -Force (Join-Path $ortExtracted "include") $runtime
Copy-Item -Recurse -Force (Join-Path $ortExtracted "lib") $runtime

$base = "https://huggingface.co/SamLowe/roberta-base-go_emotions-onnx/resolve/$modelCommit/onnx"
$modelPath = Join-Path $model "model_quantized.onnx"
Get-VerifiedFile "$base/model_quantized.onnx" $modelPath $modelSha256
$modelFiles = @{
    "config.json" = "60db0b3d640dedb02f9a033578ddecb78fa1e72ac67dd9d391606cba0e6cbcf1"
    "tokenizer.json" = "63735ef382776e869c0ee50f8e999ab19111bb794f8a451559e611077dfe7f25"
    "vocab.json" = "ed19656ea1707df69134c4af35c8ceda2cc9860bf2c3495026153a133670ab5e"
    "merges.txt" = "1ce1664773c50f3e0cc8842619a93edc4624525b728b188a9e0be33b7726adc5"
}
foreach ($name in $modelFiles.Keys) {
    $path = Join-Path $model $name
    Get-VerifiedFile "$base/$name" $path $modelFiles[$name]
}

Write-Host "affect runtime ready: $cache"
