param(
    [Parameter(Mandatory = $true)]
    [string]$SourcePath,
    [Parameter(Mandatory = $true)]
    [ValidateSet("vs_5_0", "ps_5_0")]
    [string]$Target,
    [Parameter(Mandatory = $true)]
    [string]$OutputPath,
    [Parameter(Mandatory = $true)]
    [ValidateSet("debug", "release")]
    [string]$Mode,
    [string]$FxcPath = ""
)

$ErrorActionPreference = "Stop"
if ($FxcPath.Length -gt 0) {
    $fxc = Get-Command $FxcPath -ErrorAction SilentlyContinue
    if ($null -eq $fxc) {
        throw "FXC was not found at '$FxcPath'"
    }
    $fxcPath = $fxc.Source
} else {
    $candidates = [System.Collections.Generic.List[string]]::new()
    $pathCommand = Get-Command fxc.exe -ErrorAction SilentlyContinue
    if ($null -ne $pathCommand) {
        $candidates.Add($pathCommand.Source)
    }
    if ($null -ne ${env:WindowsSdkDir} -and $null -ne ${env:WindowsSDKVersion}) {
        $sdkVersion = ${env:WindowsSDKVersion}.TrimEnd("\")
        $candidates.Add((Join-Path ${env:WindowsSdkDir} "bin\$sdkVersion\x64\fxc.exe"))
    }

    $sdkRoots = [System.Collections.Generic.List[string]]::new()
    if ($null -ne ${env:WindowsSdkDir}) {
        $sdkRoots.Add((Join-Path ${env:WindowsSdkDir} "bin"))
    }
    if ($null -ne ${env:ProgramFiles(x86)}) {
        $sdkRoots.Add((Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"))
    }
    foreach ($root in $sdkRoots) {
        if (-not (Test-Path $root)) {
            continue
        }
        $versions = Get-ChildItem -Path $root -Directory | ForEach-Object {
            $parsed = $null
            if ([version]::TryParse($_.Name, [ref]$parsed)) {
                [pscustomobject]@{ Version = $parsed; Path = $_.FullName }
            }
        } | Sort-Object Version -Descending
        foreach ($version in $versions) {
            $candidates.Add((Join-Path $version.Path "x64\fxc.exe"))
        }
    }

    $fxcPath = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($null -eq $fxcPath) {
        throw "Windows SDK fxc.exe was not found; pass FXC=/path/to/fxc.exe to make"
    }
}

$arguments = @(
    "/nologo",
    "/Ges",
    "/WX",
    "/DEIDOLON_D3D11=1",
    "/E", "main",
    "/T", $Target,
    "/Fo", $OutputPath
)
if ($Mode -eq "debug") {
    $arguments += @("/Od", "/Zi")
} else {
    $arguments += "/O3"
}
$arguments += $SourcePath

& $fxcPath @arguments
if ($LASTEXITCODE -ne 0) {
    throw "fxc failed with exit code $LASTEXITCODE"
}
