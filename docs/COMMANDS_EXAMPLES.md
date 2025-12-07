# Training Examples for Small LLMs

This file contains practical examples showing correct command usage patterns.

## Example 1: Simple Bug Fix

**Task**: Fix a typo in main.cpp where "Helo" should be "Hello"

**Correct approach**:
```json
[
  {
    "command": "REASONING",
    "params": {
      "reasoning": "First, I need to read main.cpp to find the typo and see the line numbers."
    }
  },
  {
    "command": "OPEN_FILE",
    "params": {
      "path": "src/main.cpp"
    }
  }
]
```

**After seeing output**:
```
1: #include <iostream>
2: 
3: int main() {
4:     std::cout << "Helo World" << std::endl;
5:     return 0;
6: }
```

**Continue with**:
```json
[
  {
    "command": "REASONING",
    "params": {
      "reasoning": "I found the typo on line 4. I'll fix it by replacing that line."
    }
  },
  {
    "command": "EDIT_IN_RANGE",
    "params": {
      "path": "src/main.cpp",
      "startLine": 4,
      "endLine": 4,
      "content": "    std::cout << \"Hello World\" << std::endl;"
    }
  }
]
```

---

## Example 2: Adding a New Function

**Task**: Add a new function `int multiply(int a, int b)` to utils.cpp

**Correct approach**:
```json
[
  {
    "command": "REASONING",
    "params": {
      "reasoning": "I need to see the current structure of utils.cpp to know where to add the function."
    }
  },
  {
    "command": "OPEN_FILE",
    "params": {
      "path": "src/utils.cpp"
    }
  }
]
```

**After seeing output**:
```
1: #include "utils.h"
2: 
3: int add(int a, int b) {
4:     return a + b;
5: }
6: 
7: int subtract(int a, int b) {
8:     return a - b;
9: }
```

**Continue with**:
```json
[
  {
    "command": "REASONING",
    "params": {
      "reasoning": "I'll add the multiply function after the subtract function. I'll insert it after line 9."
    }
  },
  {
    "command": "EDIT_IN_RANGE",
    "params": {
      "path": "src/utils.cpp",
      "startLine": 9,
      "endLine": 9,
      "content": "}\n\nint multiply(int a, int b) {\n    return a * b;\n}"
    }
  }
]
```

---

## Example 3: Creating Multiple Related Files

**Task**: Create a new class "Logger" with header and implementation

**Correct approach**:
```json
[
  {
    "command": "REASONING",
    "params": {
      "reasoning": "I'll create two files: Logger.h for the header and Logger.cpp for implementation."
    }
  },
  {
    "command": "CREATE_FILE",
    "params": {
      "path": "src/Logger.h",
      "content": "#pragma once\n#include <string>\n\nclass Logger {\npublic:\n    void log(const std::string& message);\n    void error(const std::string& message);\n};"
    }
  },
  {
    "command": "CREATE_FILE",
    "params": {
      "path": "src/Logger.cpp",
      "content": "#include \"Logger.h\"\n#include <iostream>\n\nvoid Logger::log(const std::string& message) {\n    std::cout << \"[LOG] \" << message << std::endl;\n}\n\nvoid Logger::error(const std::string& message) {\n    std::cerr << \"[ERROR] \" << message << std::endl;\n}"
    }
  }
]
```

---

## Example 4: Finding and Fixing a Bug

**Task**: Find where "oldFunction" is defined and rename it to "newFunction"

**Correct approach**:
```json
[
  {
    "command": "REASONING",
    "params": {
      "reasoning": "I'll search the project to find where oldFunction is defined."
    }
  },
  {
    "command": "SEARCH_PROJECT",
    "params": {
      "query": "oldFunction",
      "maxMatches": 5
    }
  }
]
```

**After seeing search results**:
```
src/utils.cpp:15
void oldFunction() {

src/utils.h:8
void oldFunction();
```

**Continue with**:
```json
[
  {
    "command": "REASONING",
    "params": {
      "reasoning": "Found it in utils.cpp at line 15 and utils.h at line 8. I'll update both files."
    }
  },
  {
    "command": "OPEN_FILE",
    "params": {
      "path": "src/utils.h"
    }
  }
]
```

**After seeing utils.h**:
```
1: #pragma once
2: 
3: int add(int a, int b);
4: int subtract(int a, int b);
5: 
6: // Utility functions
7: void oldFunction();
8: void helperFunction();
```

**Continue**:
```json
[
  {
    "command": "EDIT_IN_RANGE",
    "params": {
      "path": "src/utils.h",
      "startLine": 7,
      "endLine": 7,
      "content": "void newFunction();"
    }
  },
  {
    "command": "OPEN_FILE",
    "params": {
      "path": "src/utils.cpp"
    }
  }
]
```

---

## Example 5: Complex Multi-Step Task

**Task**: Refactor code to use a config file instead of hardcoded values

**Correct approach**:
```json
[
  {
    "command": "REASONING",
    "params": {
      "reasoning": "This is a complex task. My plan:\n1. Read the current code to find hardcoded values\n2. Create a config.h file\n3. Create a config.cpp file\n4. Update the main code to use the config\nLet me start by examining the current code."
    }
  },
  {
    "command": "OPEN_FILE",
    "params": {
      "path": "src/main.cpp"
    }
  }
]
```

**After analysis**:
```json
[
  {
    "command": "REASONING",
    "params": {
      "reasoning": "I found hardcoded values on lines 10, 15, and 20. Now I'll create the config files."
    }
  },
  {
    "command": "CREATE_FILE",
    "params": {
      "path": "src/config.h",
      "content": "#pragma once\n\nnamespace Config {\n    extern const int MAX_CONNECTIONS;\n    extern const int TIMEOUT_MS;\n    extern const char* SERVER_URL;\n}"
    }
  },
  {
    "command": "CREATE_FILE",
    "params": {
      "path": "src/config.cpp",
      "content": "#include \"config.h\"\n\nnamespace Config {\n    const int MAX_CONNECTIONS = 100;\n    const int TIMEOUT_MS = 5000;\n    const char* SERVER_URL = \"http://localhost:8080\";\n}"
    }
  },
  {
    "command": "REASONING",
    "params": {
      "reasoning": "Now I'll update main.cpp to use these config values instead of hardcoded ones."
    }
  },
  {
    "command": "EDIT_IN_RANGE",
    "params": {
      "path": "src/main.cpp",
      "startLine": 1,
      "endLine": 1,
      "content": "#include <iostream>\n#include \"config.h\""
    }
  }
]
```

---

## Common Mistakes to Avoid

### ❌ WRONG: Guessing line numbers
```json
{
  "command": "EDIT_IN_RANGE",
  "params": {
    "path": "src/main.cpp",
    "startLine": 5,
    "endLine": 5,
    "content": "new code"
  }
}
```
**Problem**: You didn't read the file first!

### ✅ CORRECT: Always read first
```json
[
  {"command": "OPEN_FILE", "params": {"path": "src/main.cpp"}},
  {"command": "EDIT_IN_RANGE", "params": {"path": "src/main.cpp", "startLine": 5, "endLine": 5, "content": "new code"}}
]
```

---

### ❌ WRONG: Using REPLACE_IN_FILE for small changes
```json
{
  "command": "REPLACE_IN_FILE",
  "params": {
    "path": "src/main.cpp",
    "content": "entire 500 line file with one typo fixed"
  }
}
```
**Problem**: Inefficient and error-prone!

### ✅ CORRECT: Use EDIT_IN_RANGE
```json
{
  "command": "EDIT_IN_RANGE",
  "params": {
    "path": "src/main.cpp",
    "startLine": 42,
    "endLine": 42,
    "content": "fixed line"
  }
}
```

---

### ❌ WRONG: No reasoning
```json
[
  {"command": "OPEN_FILE", "params": {"path": "src/main.cpp"}},
  {"command": "EDIT_IN_RANGE", "params": {"path": "src/main.cpp", "startLine": 10, "endLine": 15, "content": "code"}}
]
```
**Problem**: Hard to understand what you're doing!

### ✅ CORRECT: Explain your thinking
```json
[
  {"command": "REASONING", "params": {"reasoning": "I'll read the file to find the bug"}},
  {"command": "OPEN_FILE", "params": {"path": "src/main.cpp"}},
  {"command": "REASONING", "params": {"reasoning": "Found the bug on lines 10-15. I'll fix it."}},
  {"command": "EDIT_IN_RANGE", "params": {"path": "src/main.cpp", "startLine": 10, "endLine": 15, "content": "fixed code"}}
]
```

---

## Practice Exercises

### Exercise 1: Simple Edit
**Task**: Change "version 1.0" to "version 2.0" in config.txt

**Your solution should**:
1. Use REASONING to explain your plan
2. Use OPEN_FILE to read config.txt
3. Use REASONING to identify the line to change
4. Use EDIT_IN_RANGE to make the change

---

### Exercise 2: Add Function
**Task**: Add a `divide` function to calculator.cpp

**Your solution should**:
1. Read calculator.cpp to see the structure
2. Identify where to add the function
3. Use EDIT_IN_RANGE to insert it
4. Update calculator.h if needed

---

### Exercise 3: Find and Fix
**Task**: Find all files containing "TODO" and list them

**Your solution should**:
1. Use SEARCH_PROJECT to find TODOs
2. Use SHOW_RESULT to present findings
3. Explain what you found

---

## Remember

1. **Read → Think → Act**: Always in this order
2. **Line numbers are sacred**: Use exactly what OPEN_FILE shows
3. **Explain yourself**: Use REASONING liberally
4. **Start simple**: One command at a time when learning
5. **Check your work**: Re-read files after editing if unsure
