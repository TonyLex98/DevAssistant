# DevAssistant Templates Overview

This directory contains advanced templates for various programming tasks with built-in scheduling, planning, and systematic approaches.

## Template Categories

### 1. Core Programming Templates
- **Programmer Assistant** - Basic programming tasks with planning
- **Advanced Programmer Assistant v2.0** - Complete programming lifecycle with:
  - Task analysis and architecture review
  - Detailed implementation planning
  - Step-by-step scheduling
  - Code quality checks
  - Testing strategy
  - Documentation

### 2. Specialized Development Templates
- **Bug Fix Assistant** - Systematic bug fixing with:
  - Root cause analysis
  - Bug reproduction
  - Fix implementation
  - Regression prevention
  
- **Refactoring Assistant** - Safe code refactoring with:
  - Safety net setup
  - Incremental changes
  - Verification after each step
  - Quality improvements

- **Code Review & Study Assistant** - Code understanding with:
  - Systematic analysis
  - Quality assessment
  - Performance review
  - Security analysis

### 3. Operations Templates
- **DevOps Assistant** - Infrastructure and deployment tasks
- **System Architect Assistant** - Architecture and design tasks

## Template Dependencies and Relationships

The templates are interconnected to provide seamless workflow:

- **Advanced Programmer Assistant** serves as the base for most development tasks
- **Bug Fix Assistant** builds upon Advanced Programmer Assistant with bug-specific workflows
- **Refactoring Assistant** extends Advanced Programmer Assistant with safety measures
- **Code Review Assistant** can be used independently to understand code before other tasks
- **Architect Assistant** leverages Code Review Assistant for initial analysis
- **DevOps Assistant** operates independently for infrastructure tasks

## Solution-Level Configuration

Additional configuration files provide:
- **CompleteDeveloperWorkflow.json** - Low-level workflow configuration
- **DeveloperAssistantSuite.json** - High-level template relationships and dependencies

## Auto-Selection Rules

The system can automatically select the appropriate template based on query patterns:
- `bug|fix|error|issue` → Bug Fix Assistant
- `refactor|cleanup|improve` → Refactoring Assistant
- `review|understand|learn` → Code Review Assistant
- `deploy|build|pipeline|config` → DevOps Assistant
- `arch|design|structure` → Architect Assistant

## Usage

Each template follows a structured approach:
1. Analysis and understanding
2. Planning and scheduling
3. Implementation in steps
4. Quality verification
5. Documentation

The templates include context awareness through the {{CONTEXT_CHAIN}} placeholder and maintain state across steps to provide continuous assistance.