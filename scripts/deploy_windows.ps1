# Builds a self-contained Windows folder (and zip) from a release build:
# the program, the Qt libraries and plugins it needs (windeployqt), the MinGW runtime,
# OpenSSL for HTTPS, Qt's Russian translation and the Python helper scripts for voice.
#
#   .\scripts\deploy_windows.ps1 [-ExePath bin\DevAssistantQt.exe] [-OutDir dist\DevAssistant]
#                                [-OpenSslDir <folder with libssl-1_1-x64.dll>] [-Zip <file.zip>]
param(
    [string]$ExePath = "bin\DevAssistantQt.exe",
    [string]$OutDir = "dist\DevAssistant",
    [string]$OpenSslDir = "",
    [string]$Zip = ""
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ExePath)) {
    throw "Executable not found: $ExePath (build the release configuration first)"
}
$windeployqt = Get-Command windeployqt -ErrorAction SilentlyContinue
if (-not $windeployqt) {
    throw "windeployqt not found. Add the Qt bin folder (for example C:\Qt\5.15.2\mingw81_64\bin) to PATH."
}

$outAbs = Join-Path (Get-Location) $OutDir
if (Test-Path $outAbs) { Remove-Item -Recurse -Force $outAbs }
New-Item -ItemType Directory -Path $outAbs | Out-Null

$exeOut = Join-Path $outAbs "DevAssistantQt.exe"
Copy-Item $ExePath $exeOut -Force

# Qt libraries, plugins (platforms, styles, image formats, audio, sqldrivers), MinGW runtime
# and Qt's own Russian strings for standard dialogs.
# No --release: with MinGW builds of Qt 5.15 it makes windeployqt reject the platform plugin
# ("Unable to find the platform plugin", QTBUG-84567). The build type is detected from the exe.
& $windeployqt.Source --compiler-runtime --no-opengl-sw --translations ru $exeOut
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

# Without these the program does not start, so check rather than trust the tool.
foreach ($required in @("Qt5Core.dll", "Qt5Widgets.dll", "Qt5Sql.dll", "platforms\qwindows.dll",
                        "sqldrivers\qsqlite.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
    if (-not (Test-Path (Join-Path $outAbs $required))) { throw "windeployqt did not deploy $required" }
}

# Qt 5.15 loads OpenSSL 1.1 at run time for HTTPS (web search, remote model servers).
if ($OpenSslDir -and (Test-Path $OpenSslDir)) {
    Copy-Item (Join-Path $OpenSslDir "libssl-1_1-x64.dll") $outAbs
    Copy-Item (Join-Path $OpenSslDir "libcrypto-1_1-x64.dll") $outAbs
} else {
    Write-Warning "OpenSSL DLLs not added: HTTPS will not work. Pass -OpenSslDir."
}

# Only the scripts the program runs (voice input and text to speech).
$scriptsDst = Join-Path $outAbs "scripts"
New-Item -ItemType Directory -Path $scriptsDst | Out-Null
foreach ($f in @("run_faster_whisper_server.py", "run_tts_server.py", "run_omni_asr.py", "requirements.txt")) {
    Copy-Item (Join-Path "scripts" $f) $scriptsDst
}
Copy-Item "LICENSE" $outAbs
Copy-Item "THIRD_PARTY_NOTICES.md" $outAbs

$readme = @"
DevAssistant for Windows
========================

Run DevAssistantQt.exe. No installation is needed.

You also need a model server:
  * Ollama (https://ollama.com): install it, then for example
      ollama pull qwen2.5-coder:7b
      ollama pull nomic-embed-text     (for document search)
  * or any OpenAI-compatible server (llama.cpp, LM Studio, vLLM): choose it in Settings > Model.

Voice input and text to speech are optional and need Python 3.10+:
    python -m pip install -r scripts\requirements.txt

Source code and documentation: https://github.com/TonyLex98/DevAssistant
"@
Set-Content -Path (Join-Path $outAbs "README.txt") -Value $readme -Encoding utf8

if ($Zip) {
    if (Test-Path $Zip) { Remove-Item $Zip -Force }
    Compress-Archive -Path $outAbs -DestinationPath $Zip   # one top-level folder inside
    Write-Host "Package: $Zip"
}
Write-Host "Deployment ready: $outAbs"
