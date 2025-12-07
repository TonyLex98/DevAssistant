# Command System Improvements for Small LLMs

## Overview

This update improves the command system to make it easier for small language models to use correctly. The main improvements are:

1. **Line-numbered file output** - Files now show line numbers when read
2. **Detailed command descriptions** - Each command has clear explanations and examples
3. **Comprehensive documentation** - Three new documentation files for different use cases

## Key Changes

### 1. File Reading with Line Numbers

**Before**:
```
#include <iostream>

int main() {
    return 0;
}
```

**After**:
```
1: #include <iostream>
2: 
3: int main() {
4:     return 0;
5: }
```

This makes it crystal clear which line numbers to use in `EDIT_IN_RANGE` commands.

### 2. Enhanced Command Descriptions

Every command now includes:
- **Clear purpose statement**
- **When to use it**
- **Detailed parameter descriptions**
- **JSON examples**
- **Expected output format**

Example from `OPEN_FILE`:
```
"Reads a file and returns its content with line numbers. Use this to examine a file before editing. 
Output format: '<line_number>: <content>'. 
Example: { \"command\": \"OPEN_FILE\", \"params\": { \"path\": \"src/main.cpp\" } }"
```

### 3. Modified Commands

#### OpenFileContextCommand
- Now reads files line by line
- Prepends line number to each line
- Format: `<number>: <content>`

#### OpenFileRegionCommand
- Reads specified line range
- Returns lines with numbers
- Validates range before reading
- Includes actual content in result

#### EditInRangeCommand
- Updated description emphasizes using line numbers from OPEN_FILE
- Clear warning about line number requirements

#### All Other Commands
- Updated descriptions from Russian to English
- Added practical examples
- Included parameter explanations

## Documentation Files

### 1. COMMANDS_REFERENCE.md
**Purpose**: Complete reference guide

**Contents**:
- Detailed description of every command
- All parameters explained
- Multiple examples per command
- Common patterns for complex tasks
- Tips for small LLMs
- Error handling guide

**Use when**: You need complete information about a command

### 2. COMMANDS_QUICK_REF.md
**Purpose**: Quick lookup cheat sheet

**Contents**:
- One-line command examples
- Critical rules highlighted
- Basic workflow example
- Minimal explanations

**Use when**: You know the command but need syntax reminder

### 3. COMMANDS_EXAMPLES.md
**Purpose**: Training and learning resource

**Contents**:
- 5 complete worked examples
- Common mistakes with corrections
- Practice exercises
- Step-by-step walkthroughs

**Use when**: Learning how to use commands correctly

## Critical Rules for LLMs

### Rule 1: Always Read Before Edit
```
✅ CORRECT:
1. OPEN_FILE → see line numbers
2. EDIT_IN_RANGE → use those numbers

❌ WRONG:
1. EDIT_IN_RANGE → guess line numbers
```

### Rule 2: Use Exact Line Numbers
The line numbers from `OPEN_FILE` output are what you MUST use in `EDIT_IN_RANGE`.

### Rule 3: Explain Your Thinking
Use `REASONING` command to explain your plan before executing complex operations.

### Rule 4: Prefer Partial Edits
Use `EDIT_IN_RANGE` for small changes, not `REPLACE_IN_FILE`.

### Rule 5: Search Before Modify
Use `SEARCH_PROJECT` to find code before editing it.

## Example Workflow

### Task: Fix a typo in main.cpp

**Step 1 - Plan**:
```json
{
  "command": "REASONING",
  "params": {
    "reasoning": "I need to fix a typo. First, I'll read the file to find it and see line numbers."
  }
}
```

**Step 2 - Read**:
```json
{
  "command": "OPEN_FILE",
  "params": {
    "path": "src/main.cpp"
  }
}
```

**Output**:
```
1: #include <iostream>
2: 
3: int main() {
4:     std::cout << "Helo World" << std::endl;
5:     return 0;
6: }
```

**Step 3 - Analyze**:
```json
{
  "command": "REASONING",
  "params": {
    "reasoning": "I found the typo 'Helo' on line 4. I'll fix it to 'Hello'."
  }
}
```

**Step 4 - Fix**:
```json
{
  "command": "EDIT_IN_RANGE",
  "params": {
    "path": "src/main.cpp",
    "startLine": 4,
    "endLine": 4,
    "content": "    std::cout << \"Hello World\" << std::endl;"
  }
}
```

## Testing the Changes

To verify the improvements work:

1. **Test file reading**:
   - Use `OPEN_FILE` on any file
   - Verify line numbers appear
   - Check format is `<number>: <content>`

2. **Test file editing**:
   - Read a file with `OPEN_FILE`
   - Note a line number
   - Use `EDIT_IN_RANGE` with that exact number
   - Verify the edit worked

3. **Test command descriptions**:
   - Call `CommandRegistry::all()`
   - Check each command's `description()`
   - Verify examples are present

## Benefits for Small LLMs

1. **No guessing**: Line numbers are explicit
2. **Clear examples**: Every command shows how to use it
3. **Step-by-step patterns**: Documentation shows complete workflows
4. **Error prevention**: Descriptions warn about common mistakes
5. **Self-documenting**: Commands explain themselves

## Migration Notes

### For Existing Code
No breaking changes - all existing functionality preserved. The changes are additive:
- File reading now includes line numbers
- Descriptions are more detailed
- New documentation files added

### For LLM Prompts
Update system prompts to:
1. Reference the new documentation files
2. Emphasize the "read before edit" workflow
3. Encourage use of REASONING command
4. Point out line number requirement

## Future Improvements

Potential enhancements:
1. Add syntax highlighting hints in file output
2. Include file size and encoding info
3. Add command validation before execution
4. Create interactive tutorial mode
5. Add command history and undo support

## Files Modified

### Core Command Files
- `src/command/OpenFileContextCommand.h` - Added line numbering
- `src/command/OpenFileRegionCommand.h` - Added line numbering and content output
- `src/command/EditInRangeCommand.cpp` - Enhanced description
- `src/command/CreateFileCommand.cpp` - Enhanced description
- `src/command/ReplaceInFileCommand.cpp` - Enhanced description
- `src/command/DeleteFileCommand.cpp` - Enhanced description
- `src/command/RenameFileCommand.cpp` - Enhanced description
- `src/command/RunPythonCommand.cpp` - Enhanced description
- `src/command/RunShellCommand.cpp` - Enhanced description
- `src/command/SearchProjectCommand.cpp` - Enhanced description
- `src/command/SearchWebCommand.cpp` - Enhanced description
- `src/command/ListFilesInFolderCommand.cpp` - Enhanced description
- `src/command/ReasoningCommand.h` - Enhanced description
- `src/command/AskUserCommand.h` - Enhanced description
- `src/command/ShowResultCommand.cpp` - Enhanced description
- `src/command/CallSequenceCommand.h` - Enhanced description
- `src/command/OpenSequenceCommand.h` - Enhanced description

### Documentation Files (New)
- `docs/COMMANDS_REFERENCE.md` - Complete reference
- `docs/COMMANDS_QUICK_REF.md` - Quick reference
- `docs/COMMANDS_EXAMPLES.md` - Training examples

## Summary

These improvements make the command system significantly more accessible to small language models by:
- Making line numbers explicit and visible
- Providing clear, example-rich documentation
- Establishing consistent patterns
- Preventing common mistakes through better descriptions

The changes maintain backward compatibility while adding substantial value for LLM users.
