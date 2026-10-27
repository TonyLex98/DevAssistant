# Downloads tree-sitter and the grammars that DevAssistantQt.pro compiles into the app.
# They are not stored in the repository (see .gitignore) and land in src/uiide/.
# Versions are pinned to exact commits so every build uses the same sources.
# Usage:  powershell -ExecutionPolicy Bypass -File scripts/fetch_tree_sitter.ps1

$ErrorActionPreference = 'Stop'
$root = Join-Path $PSScriptRoot '..\src\uiide'

$repos = @(
    @{ Name = 'tree-sitter';        Url = 'https://github.com/tree-sitter/tree-sitter';        Commit = 'dfe770858e3cfd8e8b29a79ea17da390a6f60319' },
    @{ Name = 'tree-sitter-cpp';    Url = 'https://github.com/tree-sitter/tree-sitter-cpp';    Commit = 'c009222808634c1014f82438d4883753516a2c24' },
    @{ Name = 'tree-sitter-python'; Url = 'https://github.com/tree-sitter/tree-sitter-python'; Commit = '26855eabccb19c6abf499fbc5b8dc7cc9ab8bc64' },
    @{ Name = 'tree-sitter-dart';   Url = 'https://github.com/UserNobody14/tree-sitter-dart';  Commit = '0038280b774b65c1b89ba7fec50624d8baed9fe1' }
)

foreach ($r in $repos) {
    $dst = Join-Path $root $r.Name
    if (Test-Path $dst) {
        Write-Host "$($r.Name) already present, skipping"
        continue
    }
    git init --quiet $dst
    git -C $dst remote add origin $r.Url
    git -C $dst fetch --quiet --depth 1 origin $r.Commit
    if ($LASTEXITCODE -ne 0) { throw "git fetch failed for $($r.Name)" }
    git -C $dst checkout --quiet FETCH_HEAD
    Write-Host "$($r.Name) checked out at $($r.Commit.Substring(0, 7))"
}
