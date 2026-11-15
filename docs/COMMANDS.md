# Command reference

The model answers with JSON commands; DevAssistant checks each one (project folder, the step's allowed commands), shows the ones that change something for confirmation, and runs them. This file is generated from the program: `DevAssistantQt.exe --print-command-docs --markdown --out COMMANDS.md`.

| Command | Risk | What it does |
|---|---|---|
| [`ASK_USER`](#ask_user) | read-only | Asks the user a clarifying question (use it when information is missing). |
| [`OPEN_FILE_REGION`](#open_file_region) | read-only | Opens part of a file by line range or by regex and returns the lines with numbers. |
| [`CREATE_FILE`](#create_file) | changes files | Creates a new text file. Fails if the file already exists. |
| [`REPLACE_IN_FILE`](#replace_in_file) | changes files | Replaces the entire contents of an existing file. |
| [`DELETE_FILE`](#delete_file) | dangerous | Deletes a file. |
| [`RENAME_FILE`](#rename_file) | changes files | Renames or moves a file. |
| [`EDIT_IN_RANGE`](#edit_in_range) | changes files | Replaces a range of lines in a file (line numbers from OPEN_FILE/OPEN_FILE_REGION). |
| [`WRITE_DESCRIPTION`](#write_description) | changes files | Writes a text description for a file (project metadata). |
| [`OPEN_FILE`](#open_file) | read-only | Opens a file and returns its contents with line numbers. |
| [`RUN_PYTHON`](#run_python) | dangerous | Runs a Python script (scriptPath) or inline code (code) and returns stdout/stderr/exitCode. |
| [`RUN_SHELL`](#run_shell) | dangerous | Runs a shell command and returns stdout/stderr/exitCode (working folder: the project root). |
| [`SEARCH_WEB`](#search_web) | read-only | Searches the web and returns short results (title/url/snippet). |
| [`SEARCH_DOCS`](#search_docs) | read-only | Semantic search in the project's indexed documents (code, notes, specs, PDFs). Returns the closest excerpts with file names; cite them in the answer. |
| [`SEARCH_PROJECT`](#search_project) | read-only | Searches project files for text/regex and returns matches as path:line + snippet. |
| [`SEARCH_PROJECT_CASE_INSENSITIVE`](#search_project_case_insensitive) | read-only | Simplified case-insensitive project search. |
| [`SHOW_RESULT`](#show_result) | read-only | Shows the final text/answer to the user (performs no actions). |
| [`CALL_SEQUENCE`](#call_sequence) | read-only | Hands control over to another sequence/expert (orchestration). |
| [`OPEN_SEQUENCE`](#open_sequence) | read-only | Opens (loads) a sequence by name. |
| [`LIST_FILES_IN_FOLDER`](#list_files_in_folder) | read-only | Lists the files in a folder (non-recursive) with the line count of each file. |
| [`REASONING`](#reasoning) | read-only | Logs reasoning/a plan (performs no actions). |
| [`COPY_FILE`](#copy_file) | changes files | Copies a file (optionally overwriting the destination). |
| [`CREATE_DIR`](#create_dir) | changes files | Creates a directory (recursively). |
| [`DELETE_DIR`](#delete_dir) | dangerous | Deletes a directory (empty by default). |
| [`APPEND_TO_FILE`](#append_to_file) | changes files | Appends text to a file (optionally creating it). |
| [`INSERT_AT_LINE`](#insert_at_line) | changes files | Inserts text before the given 1-based line number. |
| [`APPLY_PATCH`](#apply_patch) | changes files | Applies a unified diff (like git diff) to files. |
| [`GIT_STATUS`](#git_status) | read-only | Runs 'git status' in the project root. |
| [`GIT_DIFF`](#git_diff) | read-only | Runs 'git diff' in the project root. |
| [`GIT_RESTORE_FILE`](#git_restore_file) | dangerous | Restores a file from git (like 'git restore'). |
| [`UNDO_LAST_CHANGE`](#undo_last_change) | changes files | Undoes the last applied filesystem change batch (single level). |
| [`CONTEXT_SHOW`](#context_show) | read-only | Shows current context buffer (UI-level). |
| [`CONTEXT_CLEAR`](#context_clear) | changes files | Clears context caches (UI-level). |
| [`OPEN_APP`](#open_app) | dangerous | Launches a desktop application. |
| [`OPEN_URL`](#open_url) | dangerous | Opens a URL in the default system browser. |
| [`CLIPBOARD_SET`](#clipboard_set) | changes files | Writes text to the system clipboard. |
| [`CLIPBOARD_GET`](#clipboard_get) | dangerous | Reads text from the system clipboard. |

## ASK_USER

Asks the user a clarifying question (use it when information is missing).

| Parameter | Type | Required | Description |
|---|---|---|---|
| `question` | string | yes | Question text (what exactly needs clarifying). |

- Do not add other commands: ask first, then act on the answer.

```json
{
    "command": "ASK_USER",
    "params": {
        "question": "Which file should be changed: src/main.cpp or src/app.cpp?"
    }
}
```

## OPEN_FILE_REGION

Opens part of a file by line range or by regex and returns the lines with numbers.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `path` | string | yes | File path (absolute or relative to the project root). |
| `startLine` | int | no | First line (1-based, inclusive). Requires endLine. |
| `endLine` | int | no | Last line (1-based, inclusive). Requires startLine. |
| `region` | string | no | Regex: if set, returns the matching lines (instead of a range). |

- Specify either startLine+endLine or region.

```json
{
    "command": "OPEN_FILE_REGION",
    "params": {
        "endLine": 120,
        "path": "src/main.cpp",
        "startLine": 1
    }
}
```

## CREATE_FILE

Creates a new text file. Fails if the file already exists.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `path` | string | yes | Path of the new file (absolute or relative to the project root). |
| `content` | string | yes | Full file contents (use \n for line breaks). |

- Do not use it to overwrite an existing file; use REPLACE_IN_FILE or EDIT_IN_RANGE for that.
- Missing folders are created automatically.

```json
{
    "command": "CREATE_FILE",
    "params": {
        "content": "#pragma once\n\nint add(int a, int b);\n",
        "path": "src/utils/NewHelper.h"
    }
}
```

## REPLACE_IN_FILE

Replaces the entire contents of an existing file.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `path` | string | yes | File path (absolute or relative to the project root). |
| `content` | string | yes | New full file contents (use \n). |

- To change only specific lines, use EDIT_IN_RANGE.

```json
{
    "command": "REPLACE_IN_FILE",
    "params": {
        "content": "#include <iostream>\n\nint main(){\n  std::cout << \"Hello\";\n}\n",
        "path": "src/main.cpp"
    }
}
```

## DELETE_FILE

Deletes a file.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `path` | string | yes | File path (absolute or relative to the project root). |

- Use with care. If unsure, run OPEN_FILE first.

```json
{
    "command": "DELETE_FILE",
    "params": {
        "path": "src/old/unused.cpp"
    }
}
```

## RENAME_FILE

Renames or moves a file.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `oldPath` | string | yes | Current file path. |
| `newPath` | string | yes | New file path. |

- Paths may be absolute or relative to the project root.

```json
{
    "command": "RENAME_FILE",
    "params": {
        "newPath": "src/new_name.cpp",
        "oldPath": "src/old_name.cpp"
    }
}
```

## EDIT_IN_RANGE

Replaces a range of lines in a file (line numbers from OPEN_FILE/OPEN_FILE_REGION).

| Parameter | Type | Required | Description |
|---|---|---|---|
| `path` | string | yes | File path (absolute or relative to the project root). |
| `startLine` | int | yes | First line (1-based, inclusive). |
| `endLine` | int | yes | Last line (1-based, inclusive). |
| `content` | string | no | New lines as a single text (\n). Alternative to replacementLines. |
| `replacementLines` | array | no | Array of objects {line:int, text:string}. Alternative to content. |

- Simplest: set startLine/endLine and content with the new lines.
- The range must exist in the file; otherwise it is an error.

```json
{
    "command": "EDIT_IN_RANGE",
    "params": {
        "content": "int x = 1;\nint y = 2;\n",
        "endLine": 12,
        "path": "src/main.cpp",
        "startLine": 10
    }
}
```

## WRITE_DESCRIPTION

Writes a text description for a file (project metadata).

| Parameter | Type | Required | Description |
|---|---|---|---|
| `sourceFilePath` | string | yes | File the description belongs to (absolute or relative path). |
| `description` | string | yes | File description (what it does and where it is used). |

- Useful after large changes, so the assistant finds its way around the project better.

```json
{
    "command": "WRITE_DESCRIPTION",
    "params": {
        "description": "Application entry point; initializes the UI and managers.",
        "sourceFilePath": "src/main.cpp"
    }
}
```

## OPEN_FILE

Opens a file and returns its contents with line numbers.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `path` | string | yes | File path (absolute or relative to the project root). |

- Use the line numbers from the result for EDIT_IN_RANGE.
- For a large file, use OPEN_FILE_REGION.

```json
{
    "command": "OPEN_FILE",
    "params": {
        "path": "src/main.cpp"
    }
}
```

## RUN_PYTHON

Runs a Python script (scriptPath) or inline code (code) and returns stdout/stderr/exitCode.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `scriptPath` | string | no | Path to a .py file (absolute or relative to the project root). |
| `code` | string | no | Inline Python code (run via `python -c`). |
| `cwd` | string | no | Working directory (absolute or relative to the project root). |
| `timeoutMs` | int | no | Time limit in milliseconds (default 120000). |

- Specify either scriptPath or code.
- Print the result with print().

```json
{
    "command": "RUN_PYTHON",
    "params": {
        "code": "print(1+1)\n",
        "cwd": "."
    }
}
```

## RUN_SHELL

Runs a shell command and returns stdout/stderr/exitCode (working folder: the project root).

| Parameter | Type | Required | Description |
|---|---|---|---|
| `command` | string | yes | Command to run (a single line). |
| `cwd` | string | no | Working directory (absolute or relative to the project root). |
| `timeoutMs` | int | no | Time limit in milliseconds (default 120000). |

- To search the project, prefer SEARCH_PROJECT.
- To run in another folder, set cwd.

```json
{
    "command": "RUN_SHELL",
    "params": {
        "command": "dir",
        "cwd": "."
    }
}
```

## SEARCH_WEB

Searches the web and returns short results (title/url/snippet).

| Parameter | Type | Required | Description |
|---|---|---|---|
| `query` | string | yes | Search query. |
| `maxResults` | int | no | How many results to return (1..10). |

```json
{
    "command": "SEARCH_WEB",
    "params": {
        "maxResults": 5,
        "query": "Qt 5.12 QNetworkAccessManager timeout example"
    }
}
```

## SEARCH_DOCS

Semantic search in the project's indexed documents (code, notes, specs, PDFs). Returns the closest excerpts with file names; cite them in the answer.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `query` | string | yes | What to look for, in natural language. |
| `k` | int | no | How many excerpts to return (default 5, at most 20). |

- Works after the project was indexed in Settings > Documents; otherwise it fails with that hint.

```json
{
    "command": "SEARCH_DOCS",
    "params": {
        "k": 5,
        "query": "how are settings saved"
    }
}
```

## SEARCH_PROJECT

Searches project files for text/regex and returns matches as path:line + snippet.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `query` | string | yes | Search string or regex (if useRegex=true). |
| `maxMatches` | int | no | Maximum number of matches (default 10). |
| `caseSensitive` | bool | no | Match case (default false). |
| `useRegex` | bool | no | Treat query as a regex. |
| `contextLines` | int | no | How many context lines to add around each match. |
| `include` | array<string> | no | File glob masks, e.g. ["*.cpp","*.h"]. |
| `includePaths` | array<string> | no | Limit the search to these paths/folders. |
| `excludePaths` | array<string> | no | Paths/folders to exclude. |

```json
{
    "command": "SEARCH_PROJECT",
    "params": {
        "include": [
            "*.cpp",
            "*.h"
        ],
        "maxMatches": 20,
        "query": "observeUserQuery"
    }
}
```

## SEARCH_PROJECT_CASE_INSENSITIVE

Simplified case-insensitive project search.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `query` | string | yes | Search string. |
| `maxMatches` | int | no | Maximum number of matches (default 10). |
| `include` | array<string> | no | File glob masks, e.g. ["*.cpp","*.h"]. |

```json
{
    "command": "SEARCH_PROJECT_CASE_INSENSITIVE",
    "params": {
        "include": [
            "*.cpp",
            "*.h"
        ],
        "maxMatches": 10,
        "query": "SearchWebCommand"
    }
}
```

## SHOW_RESULT

Shows the final text/answer to the user (performs no actions).

| Parameter | Type | Required | Description |
|---|---|---|---|
| `content` | string | yes | Text to show to the user. |
| `title` | string | no | Title (optional). |

```json
{
    "command": "SHOW_RESULT",
    "params": {
        "content": "Done: ...\nNext: ...",
        "title": "Summary"
    }
}
```

## CALL_SEQUENCE

Hands control over to another sequence/expert (orchestration).

| Parameter | Type | Required | Description |
|---|---|---|---|
| `sequenceName` | string | yes | Sequence name as listed in {{OPEN_SEQUENCE_INFO}}. |
| `summarize` | bool | no | Summarize before switching (if supported). |

```json
{
    "command": "CALL_SEQUENCE",
    "params": {
        "sequenceName": "InternetFinder",
        "summarize": true
    }
}
```

## OPEN_SEQUENCE

Opens (loads) a sequence by name.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `sequenceName` | string | yes | Sequence name (as listed). |

```json
{
    "command": "OPEN_SEQUENCE",
    "params": {
        "sequenceName": "InternetFinder"
    }
}
```

## LIST_FILES_IN_FOLDER

Lists the files in a folder (non-recursive) with the line count of each file.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `folderPath` | string | yes | Folder path (absolute or relative to the project root). |
| `pattern` | string | no | Name filter, e.g. "*.cpp" (optional). |

- The command lists the files in the folder (non-recursive) and counts lines in text files.

```json
{
    "command": "LIST_FILES_IN_FOLDER",
    "params": {
        "folderPath": "DevAss/src/command",
        "pattern": "*.cpp"
    }
}
```

## REASONING

Logs reasoning/a plan (performs no actions).

| Parameter | Type | Required | Description |
|---|---|---|---|
| `reasoning` | string | yes | Short plan: what to do next and why. |
| `nextCommand` | string | no | Hint: which command comes next. |
| `nextCommandParams` | object | no | Hint: params for nextCommand. |

```json
{
    "command": "REASONING",
    "params": {
        "nextCommand": "OPEN_FILE",
        "nextCommandParams": {
            "path": "src/main.cpp"
        },
        "reasoning": "First open the file, then replace the line range and check the build."
    }
}
```

## COPY_FILE

Copies a file (optionally overwriting the destination).

| Parameter | Type | Required | Description |
|---|---|---|---|
| `from` | string | yes | Source file path (relative to project root or absolute). |
| `to` | string | yes | Destination file path (relative to project root or absolute). |
| `overwrite` | bool | no | If true, overwrites destination if it exists. |

```json
{
    "command": "COPY_FILE",
    "params": {
        "from": "src/a.txt",
        "overwrite": false,
        "to": "src/b.txt"
    }
}
```

## CREATE_DIR

Creates a directory (recursively).

| Parameter | Type | Required | Description |
|---|---|---|---|
| `path` | string | yes | Directory path (relative to project root or absolute). |

```json
{
    "command": "CREATE_DIR",
    "params": {
        "path": "src/new_folder"
    }
}
```

## DELETE_DIR

Deletes a directory (empty by default).

| Parameter | Type | Required | Description |
|---|---|---|---|
| `path` | string | yes | Directory path (relative to project root or absolute). |
| `recursive` | bool | no | If true, deletes directory contents (UNDO may fail). |

```json
{
    "command": "DELETE_DIR",
    "params": {
        "path": "build/tmp",
        "recursive": false
    }
}
```

## APPEND_TO_FILE

Appends text to a file (optionally creating it).

| Parameter | Type | Required | Description |
|---|---|---|---|
| `path` | string | yes | File path. |
| `content` | string | yes | Text to append. |
| `createIfMissing` | bool | no | If true, creates file if missing (default true). |
| `ensureNewline` | bool | no | If true, inserts a newline between old content and appended content when needed. |

```json
{
    "command": "APPEND_TO_FILE",
    "params": {
        "content": "\nMore...",
        "createIfMissing": true,
        "path": "README.md"
    }
}
```

## INSERT_AT_LINE

Inserts text before the given 1-based line number.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `path` | string | yes | File path. |
| `line` | int | yes | 1-based line number to insert before. Use OPEN_FILE first. |
| `content` | string | yes | Text to insert (may be multi-line). |

```json
{
    "command": "INSERT_AT_LINE",
    "params": {
        "content": "// inserted\n",
        "line": 10,
        "path": "src/main.cpp"
    }
}
```

## APPLY_PATCH

Applies a unified diff (like git diff) to files.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `patch` | string | yes | Unified diff text. |
| `strip` | int | no | Strip N leading path components (like patch -p). Default 0. |

```json
{
    "command": "APPLY_PATCH",
    "params": {
        "patch": "diff --git a/a.txt b/a.txt\n--- a/a.txt\n+++ b/a.txt\n@@ -1 +1 @@\n-old\n+new\n",
        "strip": 0
    }
}
```

## GIT_STATUS

Runs 'git status' in the project root.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `porcelain` | bool | no | If true, uses --porcelain=v1 -b (default true). |
| `path` | string | no | Optional path filter. |

```json
{
    "command": "GIT_STATUS",
    "params": {
        "porcelain": true
    }
}
```

## GIT_DIFF

Runs 'git diff' in the project root.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `staged` | bool | no | If true, uses --staged. |
| `contextLines` | int | no | If set, uses -U<contextLines>. |
| `path` | string | no | Optional path filter. |

```json
{
    "command": "GIT_DIFF",
    "params": {
        "staged": false
    }
}
```

## GIT_RESTORE_FILE

Restores a file from git (like 'git restore').

| Parameter | Type | Required | Description |
|---|---|---|---|
| `path` | string | yes | File path. |
| `staged` | bool | no | If true, restores staged state. |
| `worktree` | bool | no | If false, attempts staged-only restore. |

- This change is performed by git; UNDO_LAST_CHANGE may not revert it.

```json
{
    "command": "GIT_RESTORE_FILE",
    "params": {
        "path": "src/main.cpp",
        "staged": false
    }
}
```

## UNDO_LAST_CHANGE

Undoes the last applied filesystem change batch (single level).

```json
{
    "command": "UNDO_LAST_CHANGE",
    "params": {
    }
}
```

## CONTEXT_SHOW

Shows current context buffer (UI-level).

| Parameter | Type | Required | Description |
|---|---|---|---|
| `maxChars` | int | no | Max characters to show (default 8000). |

```json
{
    "command": "CONTEXT_SHOW",
    "params": {
        "maxChars": 8000
    }
}
```

## CONTEXT_CLEAR

Clears context caches (UI-level).

| Parameter | Type | Required | Description |
|---|---|---|---|
| `scope` | string | no | active\|all (default active). |

```json
{
    "command": "CONTEXT_CLEAR",
    "params": {
        "scope": "active"
    }
}
```

## OPEN_APP

Launches a desktop application.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `app` | string | yes | Executable name or absolute path. |
| `args` | array|string | no | Command line arguments. |
| `cwd` | string | no | Working directory. |
| `detached` | bool | no | Launch detached (default true). |
| `waitMs` | int | no | When detached=false, max wait time for process finish (default 15000). |

```json
{
    "command": "OPEN_APP",
    "params": {
        "app": "notepad.exe",
        "args": [
            "README.md"
        ],
        "detached": true
    }
}
```

## OPEN_URL

Opens a URL in the default system browser.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `url` | string | yes | URL to open. |

```json
{
    "command": "OPEN_URL",
    "params": {
        "url": "https://example.com"
    }
}
```

## CLIPBOARD_SET

Writes text to the system clipboard.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `text` | string | yes | Text to copy. |

```json
{
    "command": "CLIPBOARD_SET",
    "params": {
        "text": "Build command copied"
    }
}
```

## CLIPBOARD_GET

Reads text from the system clipboard.

| Parameter | Type | Required | Description |
|---|---|---|---|
| `maxChars` | int | no | Limit returned text size (default 4000). |

```json
{
    "command": "CLIPBOARD_GET",
    "params": {
        "maxChars": 2000
    }
}
```
