# Developer Assistant Commands Reference

This document describes all available commands for the LLM assistant. Each command has detailed descriptions and examples to help small LLMs understand how to use them correctly.

## Important Principles

### 1. Always Use Line Numbers
When reading files with `OPEN_FILE` or `OPEN_FILE_REGION`, the output includes line numbers in format:
```
1: first line of code
2: second line of code
3: third line of code
```

**CRITICAL**: When using `EDIT_IN_RANGE`, you MUST use these exact line numbers from the file output!

### 2. Workflow for Editing Files
1. First, use `OPEN_FILE` or `OPEN_FILE_REGION` to read the file
2. Note the line numbers in the output
3. Use `EDIT_IN_RANGE` with the exact line numbers you saw

### 3. Command Format
All commands use JSON format:
```json
{
  "command": "COMMAND_NAME",
  "params": {
    "param1": "value1",
    "param2": "value2"
  },
  "description": "Optional description of what you're doing"
}
```

---

## File Reading Commands

### OPEN_FILE
**Purpose**: Reads a file and returns its content with line numbers.

**When to use**: 
- Before editing a file
- To examine file structure
- To understand existing code

**Parameters**:
- `path` (string, required): Path to the file (relative to project root or absolute)

**Example**:
```json
{
  "command": "OPEN_FILE",
  "params": {
    "path": "src/main.cpp"
  }
}
```

**Output format**:
```
1: #include <iostream>
2: 
3: int main() {
4:     std::cout << "Hello" << std::endl;
5:     return 0;
6: }
```

---

### OPEN_FILE_REGION
**Purpose**: Reads a specific range of lines from a file with line numbers.

**When to use**:
- Reading large files in chunks
- Examining specific sections of code
- When you only need part of a file

**Parameters**:
- `path` (string, required): Path to the file
- `startLine` (integer, required): First line to read (1-indexed)
- `endLine` (integer, required): Last line to read (inclusive)
- `region` (string, optional): Regex pattern to match lines

**Example**:
```json
{
  "command": "OPEN_FILE_REGION",
  "params": {
    "path": "src/main.cpp",
    "startLine": 10,
    "endLine": 50
  }
}
```

**Output format**:
```
10: void myFunction() {
11:     int x = 5;
12:     return x * 2;
13: }
```

---

## File Modification Commands

### CREATE_FILE
**Purpose**: Creates a new file with specified content.

**When to use**:
- Creating new source files
- Adding new configuration files
- Fails if file already exists (use REPLACE_IN_FILE instead)

**Parameters**:
- `path` (string, required): Path where to create the file
- `content` (string, required): Full content of the new file

**Example**:
```json
{
  "command": "CREATE_FILE",
  "params": {
    "path": "src/utils/helper.cpp",
    "content": "#include \"helper.h\"\n\nint add(int a, int b) {\n    return a + b;\n}"
  }
}
```

---

### EDIT_IN_RANGE
**Purpose**: Replaces specific lines in a file.

**CRITICAL**: You MUST use line numbers from `OPEN_FILE` or `OPEN_FILE_REGION` output!

**When to use**:
- Modifying specific parts of existing files
- Fixing bugs in particular lines
- Adding/removing code in specific locations

**Parameters**:
- `path` (string, required): Path to the file
- `startLine` (integer, required): First line to replace (from OPEN_FILE output!)
- `endLine` (integer, required): Last line to replace (from OPEN_FILE output!)
- `replacementLines` (array, required): New lines to insert

**Example workflow**:

Step 1 - Read the file:
```json
{
  "command": "OPEN_FILE",
  "params": {
    "path": "src/main.cpp"
  }
}
```

Output shows:
```
1: #include <iostream>
2: 
3: int main() {
4:     std::cout << "Hello" << std::endl;
5:     return 0;
6: }
```

Step 2 - Edit lines 4-4 (change the cout):
```json
{
  "command": "EDIT_IN_RANGE",
  "params": {
    "path": "src/main.cpp",
    "startLine": 4,
    "endLine": 4,
    "replacementLines": [
      {
        "line": 4,
        "text": "    std::cout << \"Hello, World!\" << std::endl;"
      }
    ]
  }
}
```

**Alternative format** (using content instead of replacementLines):
```json
{
  "command": "EDIT_IN_RANGE",
  "params": {
    "path": "src/main.cpp",
    "startLine": 4,
    "endLine": 4,
    "content": "    std::cout << \"Hello, World!\" << std::endl;"
  }
}
```

---

### REPLACE_IN_FILE
**Purpose**: Overwrites the ENTIRE content of an existing file.

**WARNING**: This replaces ALL file content! Use EDIT_IN_RANGE for partial edits.

**When to use**:
- Completely rewriting a small file
- Replacing configuration files
- When you need to change the entire file structure

**Parameters**:
- `path` (string, required): Path to the file
- `content` (string, required): New complete content

**Example**:
```json
{
  "command": "REPLACE_IN_FILE",
  "params": {
    "path": "config.txt",
    "content": "version=2.0\nmode=production"
  }
}
```

---

### DELETE_FILE
**Purpose**: Permanently deletes a file.

**Parameters**:
- `path` (string, required): Path to the file to delete

**Example**:
```json
{
  "command": "DELETE_FILE",
  "params": {
    "path": "src/temp.cpp"
  }
}
```

---

### RENAME_FILE
**Purpose**: Renames or moves a file.

**Parameters**:
- `oldPath` (string, required): Current file path
- `newPath` (string, required): New file path

**Example**:
```json
{
  "command": "RENAME_FILE",
  "params": {
    "oldPath": "src/old_name.cpp",
    "newPath": "src/new_name.cpp"
  }
}
```

---

## Search Commands

### SEARCH_PROJECT
**Purpose**: Searches for text in all project files.

**When to use**:
- Finding where a function is defined
- Locating all usages of a variable
- Searching for specific patterns in code

**Parameters**:
- `query` (string, required): Text to search for
- `maxMatches` (integer, optional, default: 10): Maximum results to return
- `caseSensitive` (boolean, optional, default: false): Case-sensitive search
- `useRegex` (boolean, optional, default: false): Use regex pattern
- `contextLines` (integer, optional, default: 0): Lines of context around matches

**Example**:
```json
{
  "command": "SEARCH_PROJECT",
  "params": {
    "query": "class MainWindow",
    "maxMatches": 5
  }
}
```

**Output format**:
```
/path/to/file.cpp:42
class MainWindow : public QWidget
```

---

### SEARCH_WEB
**Purpose**: Searches the internet for information.

**When to use**:
- Finding documentation
- Looking up error messages
- Getting current information

**Parameters**:
- `query` (string, required): Search query
- `maxResults` (integer, optional, default: 5): Number of results

**Example**:
```json
{
  "command": "SEARCH_WEB",
  "params": {
    "query": "Qt 6 QNetworkAccessManager example",
    "maxResults": 3
  }
}
```

---

### LIST_FILES_IN_FOLDER
**Purpose**: Lists all files in a directory with line counts.

**Parameters**:
- `folderPath` (string, required): Path to the folder

**Example**:
```json
{
  "command": "LIST_FILES_IN_FOLDER",
  "params": {
    "folderPath": "src/ui"
  }
}
```

**Output format**:
```
MainWindow.cpp (245 lines)
MainWindow.h (67 lines)
Dialog.cpp (123 lines)
```

---

## Execution Commands

### RUN_PYTHON
**Purpose**: Runs Python code or a Python script.

**Parameters**:
- `code` (string, optional): Inline Python code to execute
- `scriptPath` (string, optional): Path to Python script file

**Note**: Provide either `code` OR `scriptPath`, not both.

**Example (inline code)**:
```json
{
  "command": "RUN_PYTHON",
  "params": {
    "code": "print(2 + 2)\nprint('Hello from Python')"
  }
}
```

**Example (script file)**:
```json
{
  "command": "RUN_PYTHON",
  "params": {
    "scriptPath": "scripts/analyze.py"
  }
}
```

---

### RUN_SHELL
**Purpose**: Executes a shell command.

**Parameters**:
- `command` (string, required): Shell command to execute

**Example**:
```json
{
  "command": "RUN_SHELL",
  "params": {
    "command": "dir /b"
  }
}
```

---

## Communication Commands

### ASK_USER
**Purpose**: Asks the user a question when you need clarification.

**When to use**:
- Unclear requirements
- Multiple possible approaches
- Need user preference

**Parameters**:
- `question` (string, required): Question to ask

**Example**:
```json
{
  "command": "ASK_USER",
  "params": {
    "question": "Should I create a new file or edit the existing one?"
  }
}
```

---

### SHOW_RESULT
**Purpose**: Shows a result or message to the user.

**When to use**:
- Presenting final answers
- Showing analysis results
- Displaying important information

**Parameters**:
- `content` (string, required): Message content
- `title` (string, optional): Message title

**Example**:
```json
{
  "command": "SHOW_RESULT",
  "params": {
    "title": "Analysis Complete",
    "content": "Found 3 potential bugs:\n1. Null pointer at line 42\n2. Memory leak at line 67\n3. Uninitialized variable at line 89"
  }
}
```

---

### REASONING
**Purpose**: Logs your thought process or planning steps.

**When to use**:
- Planning complex operations
- Explaining your logic
- Breaking down tasks

**Parameters**:
- `reasoning` (string, required): Your thought process

**Example**:
```json
{
  "command": "REASONING",
  "params": {
    "reasoning": "I need to first read the file to see the current structure, then identify where to add the new function, and finally use EDIT_IN_RANGE to insert it."
  }
}
```

---

## Sequence Commands

### CALL_SEQUENCE
**Purpose**: Calls another command sequence.

**Parameters**:
- `sequenceName` (string, required): Name of the sequence to call

**Example**:
```json
{
  "command": "CALL_SEQUENCE",
  "params": {
    "sequenceName": "deploy_app"
  }
}
```

---

### OPEN_SEQUENCE
**Purpose**: Opens a different command sequence.

**Parameters**:
- `sequenceName` (string, required): Name of the sequence to open

**Example**:
```json
{
  "command": "OPEN_SEQUENCE",
  "params": {
    "sequenceName": "testing"
  }
}
```

---

## Common Patterns for Small LLMs

### Pattern 1: Reading and Editing a File

```json
[
  {
    "command": "REASONING",
    "params": {
      "reasoning": "First, I'll read the file to see its current content and line numbers."
    }
  },
  {
    "command": "OPEN_FILE",
    "params": {
      "path": "src/main.cpp"
    }
  },
  {
    "command": "REASONING",
    "params": {
      "reasoning": "Now I can see the file has 50 lines. I need to modify lines 10-15 to fix the bug."
    }
  },
  {
    "command": "EDIT_IN_RANGE",
    "params": {
      "path": "src/main.cpp",
      "startLine": 10,
      "endLine": 15,
      "content": "// Fixed code here\nint x = 5;\nreturn x;"
    }
  }
]
```

### Pattern 2: Creating a New File

```json
[
  {
    "command": "REASONING",
    "params": {
      "reasoning": "I'll create a new helper file with utility functions."
    }
  },
  {
    "command": "CREATE_FILE",
    "params": {
      "path": "src/utils/helper.h",
      "content": "#pragma once\n\nint add(int a, int b);\nint subtract(int a, int b);"
    }
  },
  {
    "command": "CREATE_FILE",
    "params": {
      "path": "src/utils/helper.cpp",
      "content": "#include \"helper.h\"\n\nint add(int a, int b) {\n    return a + b;\n}\n\nint subtract(int a, int b) {\n    return a - b;\n}"
    }
  }
]
```

### Pattern 3: Finding and Fixing Code

```json
[
  {
    "command": "REASONING",
    "params": {
      "reasoning": "I need to find where the MainWindow class is defined."
    }
  },
  {
    "command": "SEARCH_PROJECT",
    "params": {
      "query": "class MainWindow",
      "maxMatches": 3
    }
  },
  {
    "command": "REASONING",
    "params": {
      "reasoning": "Found it in src/MainWindow.h. Now I'll read that file to see the structure."
    }
  },
  {
    "command": "OPEN_FILE",
    "params": {
      "path": "src/MainWindow.h"
    }
  }
]
```

---

## Tips for Small LLMs

1. **Always read before editing**: Use `OPEN_FILE` first to see line numbers
2. **Use REASONING**: Explain your plan before executing commands
3. **Check line numbers carefully**: The line numbers from `OPEN_FILE` are what you use in `EDIT_IN_RANGE`
4. **Start simple**: Use one command at a time when learning
5. **Ask when unsure**: Use `ASK_USER` if you're not certain about something
6. **Prefer EDIT_IN_RANGE over REPLACE_IN_FILE**: Only replace entire files when necessary
7. **Use SEARCH_PROJECT**: Find code before modifying it
8. **Plan with REASONING**: Break complex tasks into steps

---

## Error Handling

If a command fails, you'll receive an error message. Common errors:

- **"File not found"**: Check the path is correct
- **"Invalid line range"**: Make sure startLine <= endLine and both are valid
- **"File already exists"**: Use `REPLACE_IN_FILE` or `EDIT_IN_RANGE` instead of `CREATE_FILE`
- **"Empty path"**: You forgot to specify the file path

When you get an error, use `REASONING` to explain what went wrong and how you'll fix it.
