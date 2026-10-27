# Сторонние компоненты

DevAssistant распространяется под лицензией MIT, см. `LICENSE`. В проекте используются или подключаются следующие сторонние компоненты.

| Компонент | Как используется | Лицензия |
|---|---|---|
| [md4c](https://github.com/mity/md4c) | исходники лежат в `src/md4c`, разбор Markdown в ответах модели | MIT, текст — `src/md4c/LICENSE.md` |
| [tree-sitter](https://github.com/tree-sitter/tree-sitter) | разбор исходников для индекса и графа проекта; скачивается скриптом `scripts/fetch_tree_sitter.ps1` | MIT |
| [tree-sitter-cpp](https://github.com/tree-sitter/tree-sitter-cpp), [tree-sitter-python](https://github.com/tree-sitter/tree-sitter-python) | грамматики C / C++ и Python; скачиваются тем же скриптом | MIT |
| [tree-sitter-dart](https://github.com/UserNobody14/tree-sitter-dart) | грамматика Dart; скачивается тем же скриптом | MIT |
| [Qt](https://www.qt.io) | фреймворк приложения, подключается динамически | LGPL v3 |
| [Ollama](https://ollama.com) | локальный сервер моделей, внешний процесс | MIT |
| [Vosk](https://alphacephei.com/vosk/) | распознавание речи, библиотека загружается во время работы, в репозитории её нет | Apache 2.0 |
| [faster-whisper](https://github.com/SYSTRAN/faster-whisper), [edge-tts](https://github.com/rany2/edge-tts), [pyttsx3](https://github.com/nateshmbhat/pyttsx3) | распознавание и синтез речи через Python-скрипты из `scripts`, ставятся отдельно | соответственно MIT, LGPL v3, MPL 2.0 |

Модели распознавания речи и языковые модели в репозиторий не входят и скачиваются пользователем отдельно.
