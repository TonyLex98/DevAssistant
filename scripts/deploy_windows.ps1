param(
    [string]$ExePath = "release\\DevAssistantQt.exe",
    [string]$OutDir = "dist\\DevAssistantQt"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ExePath)) {
    throw "Executable not found: $ExePath"
}

$exeAbs = (Resolve-Path $ExePath).Path
$outAbs = Join-Path (Get-Location) $OutDir

if (Test-Path $outAbs) {
    Remove-Item -Recurse -Force $outAbs
}
New-Item -ItemType Directory -Path $outAbs | Out-Null

$exeName = [System.IO.Path]::GetFileName($exeAbs)
$exeOut = Join-Path $outAbs $exeName
Copy-Item $exeAbs $exeOut -Force

$windeployqt = Get-Command windeployqt -ErrorAction SilentlyContinue
if (-not $windeployqt) {
    $candidates = @(
        "C:\\Qt\\Qt5.12.12\\5.12.12\\mingw73_64\\bin\\windeployqt.exe",
        "C:\\Qt\\5.12.12\\mingw73_64\\bin\\windeployqt.exe"
    )
    foreach ($cand in $candidates) {
        if (Test-Path $cand) {
            $windeployqt = @{ Source = $cand }
            break
        }
    }
}

if (-not $windeployqt) {
    throw "windeployqt not found in PATH or known Qt locations."
}

& $windeployqt.Source --release --no-translations --compiler-runtime $exeOut

$scriptsSrc = Join-Path (Get-Location) "scripts"
$scriptsDst = Join-Path $outAbs "scripts"
Copy-Item $scriptsSrc $scriptsDst -Recurse -Force

$readme = @"
DevAssistantQt deployment bundle
===============================

1. Install Python 3.10+ on target machine.
2. In this folder run:
   python -m pip install -r scripts\requirements.txt
3. Launch:
   DevAssistantQt.exe

Notes:
- If internet is blocked, pre-download ASR models (Faster Whisper) to local cache.
- TTS uses fallback chain: edge-neural -> SAPI -> pyttsx3 -> null backend.
"@

Set-Content -Path (Join-Path $outAbs "README_DEPLOY.txt") -Value $readme -Encoding utf8

Write-Host "Deployment ready: $outAbs"
