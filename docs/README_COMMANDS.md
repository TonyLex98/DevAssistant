# Command System Documentation Index

Welcome to the improved command system documentation! This index helps you find the right document for your needs.

## 📚 Documentation Files

### For Quick Reference
- **[COMMANDS_QUICK_REF.md](COMMANDS_QUICK_REF.md)** - Cheat sheet with command syntax
  - Use when: You need quick syntax lookup
  - Length: 1 page
  - Best for: Experienced users, quick reminders

### For Learning
- **[COMMANDS_EXAMPLES.md](COMMANDS_EXAMPLES.md)** - Training examples and patterns
  - Use when: Learning how to use commands
  - Length: 10 pages
  - Best for: New users, training LLMs
  - Contains: 5 complete examples, common mistakes, practice exercises

### For Complete Reference
- **[COMMANDS_REFERENCE.md](COMMANDS_REFERENCE.md)** - Full command documentation
  - Use when: You need detailed information
  - Length: 15 pages
  - Best for: Complete understanding, reference
  - Contains: All commands, all parameters, all examples

### For Integration
- **[INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md)** - How to use with LLMs
  - Use when: Setting up LLM prompts
  - Length: 8 pages
  - Best for: Prompt engineers, LLM developers
  - Contains: System prompts, testing scripts, best practices

### For Understanding Changes
- **[COMMAND_IMPROVEMENTS.md](COMMAND_IMPROVEMENTS.md)** - What changed and why
  - Use when: You want to understand the improvements
  - Length: 8 pages
  - Best for: Developers, maintainers
  - Contains: Before/after comparisons, technical details

- **[UPDATE_SUMMARY.md](UPDATE_SUMMARY.md)** - Brief summary of changes
  - Use when: You need a quick overview
  - Length: 1 page
  - Best for: Quick understanding

## 🎯 Quick Start Guide

### I'm a new user
1. Start with **[UPDATE_SUMMARY.md](UPDATE_SUMMARY.md)** - understand what's new
2. Read **[COMMANDS_EXAMPLES.md](COMMANDS_EXAMPLES.md)** - learn patterns
3. Keep **[COMMANDS_QUICK_REF.md](COMMANDS_QUICK_REF.md)** handy - for syntax

### I'm training an LLM
1. Read **[INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md)** - setup prompts
2. Use **[COMMANDS_REFERENCE.md](COMMANDS_REFERENCE.md)** - in context window
3. Test with **[COMMANDS_EXAMPLES.md](COMMANDS_EXAMPLES.md)** - validate learning

### I'm a developer
1. Read **[COMMAND_IMPROVEMENTS.md](COMMAND_IMPROVEMENTS.md)** - technical details
2. Review **[COMMANDS_REFERENCE.md](COMMANDS_REFERENCE.md)** - API reference
3. Check **[INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md)** - usage patterns

## 🔑 Key Concepts

### The Golden Rule
**Always read files before editing them!**

```
✅ CORRECT: OPEN_FILE → EDIT_IN_RANGE
❌ WRONG: EDIT_IN_RANGE (guessing line numbers)
```

### Line Numbers
Files now show line numbers:
```
1: first line
2: second line
3: third line
```

Use these exact numbers in `EDIT_IN_RANGE`!

### Command Structure
All commands use JSON:
```json
{
  "command": "COMMAND_NAME",
  "params": {
    "param1": "value1"
  }
}
```

## 📖 Command Categories

### File Reading
- `OPEN_FILE` - Read entire file with line numbers
- `OPEN_FILE_REGION` - Read specific lines
- `LIST_FILES_IN_FOLDER` - List directory contents

### File Modification
- `CREATE_FILE` - Create new file
- `EDIT_IN_RANGE` - Edit specific lines (most common!)
- `REPLACE_IN_FILE` - Replace entire file (use sparingly!)
- `DELETE_FILE` - Delete file
- `RENAME_FILE` - Rename/move file

### Search
- `SEARCH_PROJECT` - Search in project files
- `SEARCH_WEB` - Search internet

### Execution
- `RUN_PYTHON` - Run Python code
- `RUN_SHELL` - Run shell command

### Communication
- `ASK_USER` - Ask question
- `SHOW_RESULT` - Show result
- `REASONING` - Log thought process

### Sequences
- `CALL_SEQUENCE` - Call another sequence
- `OPEN_SEQUENCE` - Open another sequence

## 🎓 Learning Path

### Level 1: Basics (Start Here!)
1. Learn `OPEN_FILE` and `EDIT_IN_RANGE`
2. Practice the read-then-edit pattern
3. Use `REASONING` to explain your steps

**Resources**: 
- [COMMANDS_EXAMPLES.md](COMMANDS_EXAMPLES.md) - Example 1
- [COMMANDS_QUICK_REF.md](COMMANDS_QUICK_REF.md) - Basic syntax

### Level 2: File Operations
1. Learn `CREATE_FILE`, `DELETE_FILE`, `RENAME_FILE`
2. Practice creating and managing files
3. Understand when to use each command

**Resources**:
- [COMMANDS_EXAMPLES.md](COMMANDS_EXAMPLES.md) - Example 2, 3
- [COMMANDS_REFERENCE.md](COMMANDS_REFERENCE.md) - File commands section

### Level 3: Search and Execute
1. Learn `SEARCH_PROJECT`, `RUN_PYTHON`, `RUN_SHELL`
2. Practice finding and running code
3. Combine search with file operations

**Resources**:
- [COMMANDS_EXAMPLES.md](COMMANDS_EXAMPLES.md) - Example 4
- [COMMANDS_REFERENCE.md](COMMANDS_REFERENCE.md) - Search and Execute sections

### Level 4: Complex Workflows
1. Combine multiple commands
2. Handle complex refactoring tasks
3. Use `REASONING` for planning

**Resources**:
- [COMMANDS_EXAMPLES.md](COMMANDS_EXAMPLES.md) - Example 5
- [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md) - Advanced prompts

## 🔍 Finding Information

### "How do I...?"

| Question | Document | Section |
|----------|----------|---------|
| ...read a file? | [COMMANDS_QUICK_REF.md](COMMANDS_QUICK_REF.md) | File Operations |
| ...edit specific lines? | [COMMANDS_EXAMPLES.md](COMMANDS_EXAMPLES.md) | Example 1 |
| ...create a new file? | [COMMANDS_REFERENCE.md](COMMANDS_REFERENCE.md) | CREATE_FILE |
| ...search for code? | [COMMANDS_REFERENCE.md](COMMANDS_REFERENCE.md) | SEARCH_PROJECT |
| ...set up LLM prompts? | [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md) | System Prompt Template |
| ...understand what changed? | [COMMAND_IMPROVEMENTS.md](COMMAND_IMPROVEMENTS.md) | Key Changes |

### "What does this command do?"

See [COMMANDS_REFERENCE.md](COMMANDS_REFERENCE.md) - each command has:
- Purpose
- When to use
- Parameters
- Examples
- Output format

### "Why isn't it working?"

1. Check [COMMANDS_EXAMPLES.md](COMMANDS_EXAMPLES.md) - Common Mistakes section
2. Review [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md) - Troubleshooting section
3. Verify you're following the Golden Rule (read before edit)

## 📝 Quick Examples

### Read a file
```json
{"command": "OPEN_FILE", "params": {"path": "src/main.cpp"}}
```

### Edit a file
```json
[
  {"command": "OPEN_FILE", "params": {"path": "src/main.cpp"}},
  {"command": "EDIT_IN_RANGE", "params": {"path": "src/main.cpp", "startLine": 5, "endLine": 5, "content": "new code"}}
]
```

### Create a file
```json
{"command": "CREATE_FILE", "params": {"path": "src/new.cpp", "content": "code here"}}
```

### Search project
```json
{"command": "SEARCH_PROJECT", "params": {"query": "MainWindow", "maxMatches": 5}}
```

## 🛠️ Tools and Resources

### For Developers
- Source code: `src/command/` directory
- Command registry: `src/command/CommandRegistry.cpp`
- Command base class: `src/command/Command.h`

### For Users
- All documentation: `docs/` directory
- Quick reference: [COMMANDS_QUICK_REF.md](COMMANDS_QUICK_REF.md)
- Examples: [COMMANDS_EXAMPLES.md](COMMANDS_EXAMPLES.md)

### For LLM Integration
- Integration guide: [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md)
- System prompts: See Integration Guide
- Testing scripts: See Integration Guide

## 📞 Getting Help

1. **Check the docs**: Start with the index (this file)
2. **Read examples**: [COMMANDS_EXAMPLES.md](COMMANDS_EXAMPLES.md) has common patterns
3. **Review reference**: [COMMANDS_REFERENCE.md](COMMANDS_REFERENCE.md) has all details
4. **Test simple cases**: Start with basic commands and build up

## 🔄 Version Information

**Current Version**: 1.0 (2025-12-03)

**Changes in this version**:
- Added line numbers to file output
- Enhanced all command descriptions with examples
- Created comprehensive documentation
- Established patterns for small LLMs

See [COMMAND_IMPROVEMENTS.md](COMMAND_IMPROVEMENTS.md) for complete changelog.

## 📌 Remember

1. **Read before edit** - Always use OPEN_FILE first
2. **Use exact line numbers** - From OPEN_FILE output
3. **Explain your thinking** - Use REASONING
4. **Start simple** - Master basics before complex tasks
5. **Check the docs** - Everything is documented

---

**Happy coding! 🚀**

For detailed information, start with the document that matches your needs from the list above.
