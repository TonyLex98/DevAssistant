## DevAssistant 1.0.0

The first public release: a desktop AI assistant for programmers that works on local models
and changes the project only through commands you can see and undo.

**Download** `DevAssistant-v1.0.0-windows-x64.zip` below, unpack it and run
`DevAssistantQt.exe`. You also need a model server: [Ollama](https://ollama.com)
(for example `ollama pull qwen2.5-coder:7b`) or any OpenAI-compatible server.

### What is in it

- **Commands instead of free-form edits.** The model answers with JSON commands (open a file
  region, edit a line range, apply a diff, search the project, run a program, ask the user);
  36 commands, described in [docs/COMMANDS.md](https://github.com/TonyLex98/DevAssistant/blob/main/docs/COMMANDS.md).
- **Safe by default.**
  - Every path must stay inside the project folder.
  - Each workflow step lists the commands it may use.
  - Read-only commands run at once; changes, programs and deletions are shown first with
    the exact command line, paths and line ranges.
  - Changes are applied all-or-nothing and can be undone, up to 50 steps back.
- **Questions about your documents (RAG).** A persistent SQLite index next to the project,
  incremental updates, answers with the excerpts they rely on; the agent can search it with
  `SEARCH_DOCS`.
- **LightRAG client.** Upload documents, query in graph or vector modes, explore the graph.
- **Code insight.** An editor with highlighting, the tree-sitter syntax tree, and a project
  graph of classes, methods and calls (C/C++, Python, Dart).
- **Workflows.** Multi-step prompt sequences grouped into solution templates. An optional
  agent mode plans each step with a separate model call.
- **Voice (optional, off by default).** Dictation with faster-whisper, Vosk, whisper.cpp or
  a wav2vec2 model, and answers read aloud.
- **English and Russian interface.** It follows the system language by default; you can
  change it in Settings.

### Requirements

Windows 10 or 11, 64-bit. Voice features need Python 3.10+ and
`pip install -r scripts\requirements.txt`.
