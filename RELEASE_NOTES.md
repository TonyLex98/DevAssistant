## DevAssistant 1.1.0

A rebuilt inside: the same features on a cleaner, layered architecture, with an agent that
never freezes the window and fixes for problems the rework brought to light.

**Download** `DevAssistant-v1.1.0-windows-x64.zip` below, unpack it and run
`DevAssistantQt.exe`. You also need a model server: [Ollama](https://ollama.com)
(for example `ollama pull qwen2.5-coder:7b`) or any OpenAI-compatible server.

### What changed

- **The window stays responsive.** Model requests are asynchronous; commands that wait on a
  program or the network (shell, Python, git, web and document search) run in a worker
  thread. "Stop" works at any moment, and answers that arrive for a stopped run are dropped.
- **The model sees the files it opened.** `{{OPEN_FILES}}` was always empty before; it now
  holds the opened files and lines with their numbers, program output and search results.
- **Context usage in the status bar** shows the size of the last prompt against the model's
  context window (it showed 0 before).
- **No modal pop-ups at the end of a run**: the outcome goes to the status bar and the log.
- **Agent mode uses the model selected in the chat**, as the setting says.
- **Model servers that need a key** (OpenAI-compatible): the key from the settings is sent.
- **File descriptions** are generated in the background with a progress window that can be
  cancelled.

### Architecture

- `src/core` is a static library without QtWidgets: the session state, an asynchronous model
  client (`LlmClient`, `ModelReply`), the agent engine (`AgentEngine`, `CommandRunner`,
  `LoopGuard`, `WorkflowLibrary`), the commands, the context, the project, RAG.
- `src/ui` only builds the widgets and connects them; the main window went from 2300 lines
  and ten "service" interfaces to a few hundred lines.
- 12 test suites, among them the agent engine end to end with a scripted model on a
  temporary project, and the model client against a fake HTTP server.

See [docs/ARCHITECTURE.md](https://github.com/TonyLex98/DevAssistant/blob/main/docs/ARCHITECTURE.md).

### Requirements

Windows 10 or 11, 64-bit. Voice features need Python 3.10+ and
`pip install -r scripts\requirements.txt`.
