$ErrorActionPreference = "Stop"

function Invoke-Ollama {
  param(
    [Parameter(Mandatory=$true)][string]$Model,
    [Parameter(Mandatory=$true)][string]$Prompt
  )

  $tmpDir = [System.IO.Path]::GetTempPath()
  $promptFile = Join-Path $tmpDir ("da_prompt_{0}.txt" -f ([guid]::NewGuid().ToString("N")))
  $outFile = Join-Path $tmpDir ("da_out_{0}.txt" -f ([guid]::NewGuid().ToString("N")))

  # Use ASCII prompts to avoid encoding issues with cmd/type.
  Set-Content -Path $promptFile -Value $Prompt -Encoding Ascii

  $cmd = "type `"$promptFile`" | ollama run $Model > `"$outFile`" 2>&1"
  cmd /c $cmd | Out-Null

  if (-not (Test-Path $outFile)) { return "" }
  $text = Get-Content -Path $outFile -Raw -ErrorAction SilentlyContinue
  Remove-Item -Path $promptFile -Force -ErrorAction SilentlyContinue
  Remove-Item -Path $outFile -Force -ErrorAction SilentlyContinue

  # Strip ANSI escape sequences and spinner artifacts
  $text = [regex]::Replace($text, "\x1b\[[0-9;?]*[A-Za-z]", "")
  $text = [regex]::Replace($text, "\x1b\][^\x07]*\x07", "")
  return $text.Trim()
}

function Extract-JsonArray {
  param([Parameter(Mandatory=$true)][string]$Text)

  $start = $Text.IndexOf('[')
  $end = $Text.LastIndexOf(']')
  if ($start -lt 0 -or $end -le $start) { return $null }
  return $Text.Substring($start, $end - $start + 1)
}

function Validate-Commands {
  param([Parameter(Mandatory=$true)]$Parsed)

  if ($Parsed -isnot [System.Array]) { throw "Not a JSON array" }
  foreach ($cmd in $Parsed) {
    if (-not $cmd.command) { throw "Missing .command" }
    if (-not $cmd.params) { throw "Missing .params" }
    if ($cmd.params -is [string]) { throw ".params is a string (expected object)" }

    switch ($cmd.command) {
      "CREATE_FILE" {
        if (-not $cmd.params.path) { throw "CREATE_FILE missing params.path" }
        if (-not $cmd.params.content) { throw "CREATE_FILE missing params.content" }
      }
      "SEARCH_PROJECT" {
        if (-not $cmd.params.query) { throw "SEARCH_PROJECT missing params.query" }
      }
      "LIST_FILES_IN_FOLDER" {
        if (-not $cmd.params.folderPath) { throw "LIST_FILES_IN_FOLDER missing params.folderPath" }
      }
      default {
        # allow other commands without strict validation here
      }
    }
  }
}

function Run-Case {
  param(
    [Parameter(Mandatory=$true)][string]$Model,
    [Parameter(Mandatory=$true)][string]$Name,
    [Parameter(Mandatory=$true)][string]$Prompt
  )

  Write-Host "== $Model :: $Name ==" -ForegroundColor Cyan
  $raw = Invoke-Ollama -Model $Model -Prompt $Prompt
  if (-not $raw) {
    Write-Host "FAIL: Empty response" -ForegroundColor Red
    return $false
  }

  $json = Extract-JsonArray -Text $raw
  if (-not $json) {
    Write-Host "FAIL: No JSON array found" -ForegroundColor Red
    Write-Host "--- raw ---"
    Write-Host $raw
    return $false
  }

  try {
    $parsed = $json | ConvertFrom-Json
    Validate-Commands -Parsed $parsed
    Write-Host "PASS" -ForegroundColor Green
    return $true
  } catch {
    Write-Host ("FAIL: " + $_.Exception.Message) -ForegroundColor Red
    Write-Host "--- json ---"
    Write-Host $json
    Write-Host "--- raw ---"
    Write-Host $raw
    return $false
  }
}

$models = @(
  "gemma3:4b",
  "qwen3-vl:8b-instruct-q4_K_M"
)

$caseCreateFile = @"
You are an assistant. Reply with ONLY a JSON array of commands. No extra text.

Allowed commands:
- CREATE_FILE
  JSON: {"command":"CREATE_FILE","params":{"path":"src/new.txt","content":"hello\n"}}

Task: create file "src/tmp_smoke_test.txt" with content "hello world\n".
Reply: JSON array with exactly 1 command.
"@

$caseSearchProject = @"
You are an assistant. Reply with ONLY a JSON array of commands. No extra text.

Allowed commands:
- SEARCH_PROJECT
  JSON: {"command":"SEARCH_PROJECT","params":{"query":"SearchWebCommand","maxMatches":5,"include":["*.cpp","*.h"]}}

Task: search the project for "SearchWebCommand" (max 5 matches) only in *.cpp and *.h.
Reply: JSON array with exactly 1 command.
"@

$caseListFolder = @"
You are an assistant. Reply with ONLY a JSON array of commands. No extra text.

Allowed commands:
- LIST_FILES_IN_FOLDER
  JSON: {"command":"LIST_FILES_IN_FOLDER","params":{"folderPath":"DevAss/src/command","pattern":"*.cpp"}}

Task: list *.cpp files in folder "DevAss/src/command".
Reply: JSON array with exactly 1 command.
"@

foreach ($m in $models) {
  [void](Run-Case -Model $m -Name "CREATE_FILE" -Prompt $caseCreateFile)
  [void](Run-Case -Model $m -Name "SEARCH_PROJECT" -Prompt $caseSearchProject)
  [void](Run-Case -Model $m -Name "LIST_FILES_IN_FOLDER" -Prompt $caseListFolder)
}

Write-Host "Smoke tests completed." -ForegroundColor Green
