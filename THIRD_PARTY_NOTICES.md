# Third-party components

DevAssistant's own code is under the MIT license (see [LICENSE](LICENSE)). It uses or works
with the components below, each under its own license.

## Included in the source or the Windows package

| Component | How it is used | License |
|---|---|---|
| [Qt 5.15](https://www.qt.io) (Core, Gui, Widgets, Network, Multimedia, Sql with the SQLite driver) | application framework; the Windows package ships the unmodified Qt DLLs, linked dynamically | LGPL v3 |
| [md4c](https://github.com/mity/md4c) | Markdown parser for model answers, source in `src/md4c` | MIT, see `src/md4c/LICENSE.md` |
| [tree-sitter](https://github.com/tree-sitter/tree-sitter) with the [C++](https://github.com/tree-sitter/tree-sitter-cpp), [Python](https://github.com/tree-sitter/tree-sitter-python) and [Dart](https://github.com/UserNobody14/tree-sitter-dart) grammars | parsing source code for the project graph and the editor; downloaded at pinned commits by `scripts/fetch_tree_sitter.ps1` and compiled into the program | MIT |
| [OpenSSL 1.1.1](https://www.openssl.org) | HTTPS for web search and remote model servers; the Windows package ships `libssl-1_1-x64.dll` and `libcrypto-1_1-x64.dll` | OpenSSL and SSLeay licenses |
| MinGW-w64 runtime (`libgcc`, `libstdc++`, `libwinpthread`) | C/C++ runtime of the Windows build | GPL v3 with the GCC Runtime Library Exception; MIT/ZPL for winpthreads |

The Qt libraries are used under the LGPL v3. Their source code is available at
https://download.qt.io/archive/qt/5.15/5.15.2/single/ ; you may replace the Qt DLLs in the
package with your own build of the same version.

## External programs and services (installed separately)

| Component | What for | License |
|---|---|---|
| [Ollama](https://ollama.com) or any OpenAI-compatible server (llama.cpp, LM Studio, vLLM) | running the language and embedding models | MIT (Ollama, llama.cpp) |
| [LightRAG](https://github.com/HKUDS/LightRAG) server | the optional LightRAG window | MIT |
| [Poppler](https://poppler.freedesktop.org) `pdftotext` | reading PDF files for document search | GPL v2 |
| [faster-whisper](https://github.com/SYSTRAN/faster-whisper) | voice input through `scripts/run_faster_whisper_server.py` | MIT |
| [Vosk](https://alphacephei.com/vosk/) | offline voice input, `libvosk.dll` loaded at run time | Apache 2.0 |
| [whisper.cpp](https://github.com/ggerganov/whisper.cpp) | voice input through its command-line program | MIT |
| [Transformers](https://github.com/huggingface/transformers), [PyTorch](https://pytorch.org) | Omni ASR voice input (`scripts/run_omni_asr.py`) | Apache 2.0, BSD-3-Clause |
| [edge-tts](https://github.com/rany2/edge-tts), [pyttsx3](https://github.com/nateshmbhat/pyttsx3) | text to speech through `scripts/run_tts_server.py` | LGPL v3, MPL 2.0 |

Language, embedding and speech models are not part of the repository or the package; each
has its own license, which the user accepts when downloading it.
