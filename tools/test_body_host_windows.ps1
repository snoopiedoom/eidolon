param(
    [Parameter(Mandatory = $true)]
    [string]$ApplicationPath,

    [Parameter(Mandatory = $true)]
    [string]$PortraitConfigPath
)

$ErrorActionPreference = "Stop"
$application = (Resolve-Path -LiteralPath $ApplicationPath).Path
$portraitConfig = (Resolve-Path -LiteralPath $PortraitConfigPath).Path
$logPath = Join-Path ([Environment]::GetFolderPath("LocalApplicationData")) "Eidolon\eidolon.log"

function Invoke-BodyHostProbe {
    param(
        [string]$Name,
        [hashtable]$Variables,
        [string[]]$RequiredPatterns
    )

    $logOffset = if (Test-Path -LiteralPath $logPath) {
        (Get-Item -LiteralPath $logPath).Length
    } else {
        0L
    }
    $saved = @{}
    foreach ($key in $Variables.Keys) {
        $saved[$key] = [Environment]::GetEnvironmentVariable($key, "Process")
        [Environment]::SetEnvironmentVariable($key, [string]$Variables[$key], "Process")
    }
    try {
        $process = Start-Process -FilePath $application -PassThru -WindowStyle Hidden
        if (-not $process.WaitForExit(45000)) {
            Stop-Process -Id $process.Id -Force
            throw "$Name timed out after 45 seconds"
        }
        if ($process.ExitCode -ne 0) {
            throw "$Name exited with code $($process.ExitCode)"
        }
    } finally {
        foreach ($key in $Variables.Keys) {
            [Environment]::SetEnvironmentVariable($key, $saved[$key], "Process")
        }
    }

    $stream = [System.IO.File]::Open($logPath, [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    try {
        [void]$stream.Seek($logOffset, [System.IO.SeekOrigin]::Begin)
        $reader = New-Object System.IO.StreamReader($stream)
        try {
            $log = $reader.ReadToEnd()
        } finally {
            $reader.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
    foreach ($pattern in $RequiredPatterns) {
        if ($log -notmatch $pattern) {
            throw "$Name did not produce required evidence: $pattern`n$log"
        }
    }
    Write-Host "body-host probe passed: $Name"
}

$common = @{
    EIDOLON_PRESENTATION_TEST_HIDDEN = "1"
    EIDOLON_PRESENTATION_TEST_IGNORE_USER_SETTINGS = "1"
    EIDOLON_PRESENTATION_TEST_CHARACTER_CONFIG = $portraitConfig
}

function New-ProbeEnvironment {
    param([hashtable]$Additional)
    $result = @{}
    foreach ($key in $common.Keys) {
        $result[$key] = $common[$key]
    }
    foreach ($key in $Additional.Keys) {
        $result[$key] = $Additional[$key]
    }
    return $result
}

Invoke-BodyHostProbe "native live matrix" (New-ProbeEnvironment @{
    EIDOLON_BODY_RENDERER = "sprite"
    EIDOLON_PRESENTATION_BACKEND = "win32_dcomp"
    EIDOLON_PRESENTATION_TEST_BODY_SEQUENCE = "portrait,model_3d,sprite"
    EIDOLON_PRESENTATION_TEST_EXIT_AFTER_BODY_SEQUENCE = "1"
}) @(
    "sprite target redraw",
    "live body switch requested=2D Portrait .*state_preserved=yes outgoing_frame=valid backend=win32_dcomp",
    "live body switch requested=3D Model .*state_preserved=yes outgoing_frame=valid backend=win32_dcomp",
    "live body switch requested=Sprite .*state_preserved=yes outgoing_frame=valid backend=win32_dcomp"
)

Invoke-BodyHostProbe "legacy live matrix" (New-ProbeEnvironment @{
    EIDOLON_BODY_RENDERER = "sprite"
    EIDOLON_PRESENTATION_BACKEND = "sdl_window_legacy"
    EIDOLON_PRESENTATION_TEST_BODY_SEQUENCE = "portrait,model_3d,sprite"
    EIDOLON_PRESENTATION_TEST_EXIT_AFTER_BODY_SEQUENCE = "1"
}) @(
    "live body switch requested=2D Portrait .*state_preserved=yes outgoing_frame=valid backend=sdl_window_legacy",
    "live body switch requested=3D Model .*state_preserved=yes outgoing_frame=valid backend=sdl_window_legacy",
    "live body switch requested=Sprite .*state_preserved=yes outgoing_frame=valid backend=sdl_window_legacy"
)

foreach ($body in @("sprite", "portrait", "model_3d")) {
    Invoke-BodyHostProbe "native recovery $body" (New-ProbeEnvironment @{
        EIDOLON_BODY_RENDERER = $body
        EIDOLON_PRESENTATION_BACKEND = "win32_dcomp"
        EIDOLON_DCOMP_TEST_RESET_AFTER_FRAMES = "2"
        EIDOLON_PRESENTATION_TEST_EXIT_AFTER_RECOVERY = "1"
    }) @(
        "presentation recovered requested=win32_dcomp active=win32_dcomp reset=device"
    )

    Invoke-BodyHostProbe "fallback recovery $body" (New-ProbeEnvironment @{
        EIDOLON_BODY_RENDERER = $body
        EIDOLON_PRESENTATION_BACKEND = "win32_dcomp"
        EIDOLON_DCOMP_TEST_RESET_AFTER_FRAMES = "2"
        EIDOLON_DCOMP_TEST_FORCE_RECOVERY_FALLBACK = "1"
        EIDOLON_PRESENTATION_TEST_EXIT_AFTER_RECOVERY = "1"
    }) @(
        "presentation fallback requested=win32_dcomp active=sdl_window_legacy reset=device"
    )
}

Write-Host "Windows body-host matrix passed"
