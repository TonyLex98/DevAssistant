# Commands Quick Reference

## File Operations

### Read Files (Always First!)
```json
{"command": "OPEN_FILE", "params": {"path": "src/file.cpp"}}
{"command": "OPEN_FILE_REGION", "params": {"path": "src/file.cpp", "startLine": 10, "endLine": 50}}
```

### Edit Files (Use line numbers from OPEN_FILE!)
```json
{"command": "EDIT_IN_RANGE", "params": {"path": "src/file.cpp", "startLine": 10, "endLine": 12, "content": "new code"}}
```

### Create/Delete Files
```json
{"command": "CREATE_FILE", "params": {"path": "src/new.cpp", "content": "code here"}}
{"command": "DELETE_FILE", "params": {"path": "src/old.cpp"}}
{"command": "RENAME_FILE", "params": {"oldPath": "old.cpp", "newPath": "new.cpp"}}
{"command": "REPLACE_IN_FILE", "params": {"path": "file.cpp", "content": "entire new content"}}
```

## Search
```json
{"command": "SEARCH_PROJECT", "params": {"query": "MainWindow", "maxMatches": 5}}
{"command": "SEARCH_WEB", "params": {"query": "Qt documentation", "maxResults": 3}}
{"command": "LIST_FILES_IN_FOLDER", "params": {"folderPath": "src"}}
```

## Execute
```json
{"command": "RUN_PYTHON", "params": {"code": "print(2+2)"}}
{"command": "RUN_SHELL", "params": {"command": "dir"}}
```

## Communication
```json
{"command": "ASK_USER", "params": {"question": "Which approach?"}}
{"command": "SHOW_RESULT", "params": {"title": "Done", "content": "Results here"}}
{"command": "REASONING", "params": {"reasoning": "My plan is..."}}
```

## CRITICAL RULES

1. **ALWAYS** use `OPEN_FILE` before `EDIT_IN_RANGE`
2. **ALWAYS** use the line numbers from `OPEN_FILE` output
3. **NEVER** guess line numbers
4. Use `REASONING` to explain your plan
5. Use `EDIT_IN_RANGE` for partial edits, not `REPLACE_IN_FILE`

## Example Workflow

```json
[
  {"command": "REASONING", "params": {"reasoning": "I'll read the file first"}},
  {"command": "OPEN_FILE", "params": {"path": "src/main.cpp"}},
  {"command": "REASONING", "params": {"reasoning": "I see line 10 has the bug. I'll fix it."}},
  {"command": "EDIT_IN_RANGE", "params": {"path": "src/main.cpp", "startLine": 10, "endLine": 10, "content": "fixed code"}}
]
```
