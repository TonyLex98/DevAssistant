# Integration Guide: Using Improved Commands with LLMs

## Overview

This guide explains how to integrate the improved command system into your LLM prompts and workflows.

## System Prompt Template

Add this to your LLM system prompt:

```
You are a developer assistant with access to file manipulation commands.

CRITICAL RULES:
1. ALWAYS use OPEN_FILE before EDIT_IN_RANGE
2. ALWAYS use the exact line numbers shown in OPEN_FILE output
3. NEVER guess line numbers
4. Use REASONING to explain your plan before complex operations

FILE OUTPUT FORMAT:
When you use OPEN_FILE, you'll see:
1: first line
2: second line
3: third line

Use these numbers (1, 2, 3) in EDIT_IN_RANGE commands!

AVAILABLE COMMANDS:
- OPEN_FILE: Read file with line numbers
- OPEN_FILE_REGION: Read specific lines
- EDIT_IN_RANGE: Edit specific lines (requires line numbers from OPEN_FILE!)
- CREATE_FILE: Create new file
- REPLACE_IN_FILE: Replace entire file (use sparingly!)
- DELETE_FILE: Delete file
- RENAME_FILE: Rename/move file
- SEARCH_PROJECT: Search in project files
- SEARCH_WEB: Search internet
- LIST_FILES_IN_FOLDER: List directory contents
- RUN_PYTHON: Execute Python code
- RUN_SHELL: Execute shell command
- ASK_USER: Ask user a question
- SHOW_RESULT: Show result to user
- REASONING: Log your thought process

WORKFLOW PATTERN:
1. Use REASONING to plan
2. Use OPEN_FILE to read
3. Use REASONING to analyze
4. Use EDIT_IN_RANGE to modify

See COMMANDS_REFERENCE.md for complete documentation.
```

## Context Window Optimization

### For Small Context Windows (< 8K tokens)

Include only the quick reference:

```
Quick Command Reference:

Read: {"command": "OPEN_FILE", "params": {"path": "file.cpp"}}
Edit: {"command": "EDIT_IN_RANGE", "params": {"path": "file.cpp", "startLine": 5, "endLine": 5, "content": "new"}}
Create: {"command": "CREATE_FILE", "params": {"path": "new.cpp", "content": "code"}}

RULE: Always OPEN_FILE before EDIT_IN_RANGE!
```

### For Medium Context Windows (8K - 32K tokens)

Include the quick reference + key examples:

```
[Include COMMANDS_QUICK_REF.md content]

Example workflow:
1. OPEN_FILE → see line numbers
2. EDIT_IN_RANGE → use those numbers

[Include 1-2 examples from COMMANDS_EXAMPLES.md]
```

### For Large Context Windows (> 32K tokens)

Include the full reference:

```
[Include COMMANDS_REFERENCE.md content]
```

## Training Approach

### Phase 1: Basic Commands
Train the LLM on:
- OPEN_FILE
- EDIT_IN_RANGE
- REASONING

Focus on the read-then-edit pattern.

### Phase 2: File Operations
Add:
- CREATE_FILE
- DELETE_FILE
- RENAME_FILE

Practice creating and managing files.

### Phase 3: Search and Execute
Add:
- SEARCH_PROJECT
- RUN_PYTHON
- RUN_SHELL

Practice finding and running code.

### Phase 4: Advanced Workflows
Combine all commands for complex tasks.

## Example Prompts

### Beginner Prompt
```
Task: Fix the typo in main.cpp where "Helo" should be "Hello"

Remember:
1. Read the file first with OPEN_FILE
2. Find the line number with the typo
3. Use EDIT_IN_RANGE with that line number

Start with a REASONING command to explain your plan.
```

### Intermediate Prompt
```
Task: Add a new function 'multiply' to calculator.cpp

Steps:
1. Read calculator.cpp to see its structure
2. Decide where to add the function
3. Use EDIT_IN_RANGE to insert it
4. Update calculator.h if needed

Use REASONING commands to explain each step.
```

### Advanced Prompt
```
Task: Refactor the authentication system to use a separate Auth class

This requires:
1. Analyzing existing code
2. Creating new files
3. Modifying existing files
4. Testing the changes

Plan your approach with REASONING, then execute step by step.
```

## Error Handling

### Common Errors and Solutions

**Error**: "Invalid line range"
**Cause**: Used wrong line numbers
**Solution**: Always use OPEN_FILE first

**Error**: "File not found"
**Cause**: Wrong path
**Solution**: Use SEARCH_PROJECT or LIST_FILES_IN_FOLDER to find the file

**Error**: "File already exists"
**Cause**: Used CREATE_FILE on existing file
**Solution**: Use EDIT_IN_RANGE or REPLACE_IN_FILE instead

## Validation Checklist

Before deploying, verify:

- [ ] LLM reads files before editing
- [ ] LLM uses correct line numbers
- [ ] LLM explains its reasoning
- [ ] LLM handles errors gracefully
- [ ] LLM asks user when unsure

## Testing Script

Use this to test the LLM:

```
Test 1: Simple Read
Prompt: "Read src/main.cpp and tell me what it does"
Expected: Uses OPEN_FILE, explains the code

Test 2: Simple Edit
Prompt: "Change line 5 in main.cpp to print 'Hello World'"
Expected: Uses OPEN_FILE first, then EDIT_IN_RANGE with correct line number

Test 3: Create File
Prompt: "Create a new file utils.h with a function declaration"
Expected: Uses CREATE_FILE with proper content

Test 4: Search and Modify
Prompt: "Find where 'oldFunction' is defined and rename it to 'newFunction'"
Expected: Uses SEARCH_PROJECT, then OPEN_FILE, then EDIT_IN_RANGE

Test 5: Complex Task
Prompt: "Add error handling to all functions in processor.cpp"
Expected: Uses REASONING to plan, OPEN_FILE to read, multiple EDIT_IN_RANGE commands
```

## Monitoring and Improvement

### Metrics to Track

1. **Command success rate**: % of commands that execute correctly
2. **Read-before-edit compliance**: % of edits preceded by reads
3. **Line number accuracy**: % of EDIT_IN_RANGE with correct line numbers
4. **Reasoning usage**: % of complex tasks with REASONING commands

### Improvement Cycle

1. **Collect data**: Track command usage and errors
2. **Identify patterns**: Find common mistakes
3. **Update prompts**: Add specific guidance for common errors
4. **Retrain**: Update examples based on real usage
5. **Repeat**: Continuous improvement

## Best Practices

### For Prompt Engineers

1. **Be explicit**: State the read-before-edit rule clearly
2. **Show examples**: Include 2-3 complete workflows
3. **Emphasize line numbers**: Repeat this requirement
4. **Encourage reasoning**: Ask for explanations

### For LLM Developers

1. **Start simple**: Begin with basic commands
2. **Build gradually**: Add complexity incrementally
3. **Test thoroughly**: Verify each command works
4. **Monitor usage**: Track how commands are used

### For Users

1. **Provide clear tasks**: Specific requirements help
2. **Review outputs**: Check the LLM's work
3. **Give feedback**: Help improve the system
4. **Be patient**: Small LLMs need clear guidance

## Troubleshooting

### LLM Not Reading Files First

**Solution**: Add to prompt:
```
MANDATORY: Before any EDIT_IN_RANGE command, you MUST use OPEN_FILE.
This is not optional. Always read first.
```

### LLM Using Wrong Line Numbers

**Solution**: Add to prompt:
```
The line numbers in OPEN_FILE output are what you use in EDIT_IN_RANGE.
If you see "5: code here", use startLine: 5 and endLine: 5.
```

### LLM Not Explaining Reasoning

**Solution**: Add to prompt:
```
Before each command, use REASONING to explain:
- What you're about to do
- Why you're doing it
- What you expect to happen
```

## Resources

- **COMMANDS_REFERENCE.md**: Complete command documentation
- **COMMANDS_QUICK_REF.md**: Quick lookup cheat sheet
- **COMMANDS_EXAMPLES.md**: Training examples and patterns
- **COMMAND_IMPROVEMENTS.md**: Detailed change documentation

## Support

For issues or questions:
1. Check the documentation files
2. Review the examples
3. Test with simple cases first
4. Gradually increase complexity

## Version History

- **v1.0** (2025-12-03): Initial release with line numbering and enhanced descriptions
  - Added line numbers to file output
  - Enhanced all command descriptions
  - Created comprehensive documentation
  - Established patterns for small LLMs
