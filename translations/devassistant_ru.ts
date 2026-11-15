<?xml version='1.0' encoding='utf-8'?>
<TS version="2.1" language="ru_RU">
<context>
    <name>AgentEngine</name>
    <message>
        <source>The model did not answer: %1</source>
        <translation>Модель не ответила: %1</translation>
    </message>
    <message>
        <source>Template execution stopped by the user.</source>
        <translation>Выполнение шаблона остановлено пользователем.</translation>
    </message>
    <message>
        <source>Template execution was stopped by the user.</source>
        <translation>Выполнение шаблона остановлено пользователем.</translation>
    </message>
    <message>
        <source>Model error: %1</source>
        <translation>Ошибка модели: %1</translation>
    </message>
    <message>
        <source>Agent mode: the planning call failed (%1); asking for the commands directly.</source>
        <translation>Режим агента: запрос плана не удался (%1); прошу команды напрямую.</translation>
    </message>
    <message>
        <source>No JSON found in the model's answer; it was saved as context. Trying to parse the raw answer.</source>
        <translation>В ответе модели нет JSON; ответ сохранён в контекст. Пробую разобрать его как есть.</translation>
    </message>
    <message>
        <source>The model returned no commands. The raw answer was added to the context.</source>
        <translation>Модель не вернула команд. Её ответ добавлен в контекст.</translation>
    </message>
    <message>
        <source>Rejected commands:</source>
        <translation>Отклонённые команды:</translation>
    </message>
    <message>
        <source>the model keeps proposing commands that are not allowed</source>
        <translation>модель продолжает предлагать недопустимые команды</translation>
    </message>
    <message>
        <source>The commands were declined.</source>
        <translation>Команды отклонены.</translation>
    </message>
    <message>
        <source>Answer from %1:</source>
        <translation>Ответ от %1:</translation>
    </message>
    <message>
        <source>SHOW_RESULT was called without text.</source>
        <translation>SHOW_RESULT вызван без текста.</translation>
    </message>
    <message>
        <source>Context</source>
        <translation>Контекст</translation>
    </message>
    <message>
        <source>Execution error: %1</source>
        <translation>Ошибка выполнения: %1</translation>
    </message>
    <message>
        <source>Execution error</source>
        <translation>Ошибка выполнения</translation>
    </message>
    <message>
        <source>Sequence not found: %1</source>
        <translation>Последовательность не найдена: %1</translation>
    </message>
    <message>
        <source>The sequence has finished.</source>
        <translation>Последовательность выполнена полностью.</translation>
    </message>
    <message>
        <source>I stopped automatically so as not to run in a loop.</source>
        <translation>Я завершил выполнение автоматически, чтобы не допустить зацикливания.</translation>
    </message>
    <message>
        <source>Request: %1</source>
        <translation>Запрос: %1</translation>
    </message>
    <message>
        <source>Sources found:</source>
        <translation>Найденные источники:</translation>
    </message>
    <message>
        <source>Last commands run: %1</source>
        <translation>Последние выполненные команды: %1</translation>
    </message>
    <message>
        <source>If needed, make the request a little more specific: period, region, sources.</source>
        <translation>Если нужно, сформулируйте запрос чуть точнее: период, регион, источники.</translation>
    </message>
    <message>
        <source>Model's answer</source>
        <translation>Ответ нейросети</translation>
    </message>
</context>
<context>
    <name>ChatPanel</name>
    <message>
        <source>Send</source>
        <translation>Отправить</translation>
    </message>
    <message>
        <source>Stop</source>
        <translation>Остановить</translation>
    </message>
    <message>
        <source>Image</source>
        <translation>Фото</translation>
    </message>
    <message>
        <source>Attach image (png/jpg/webp)</source>
        <translation>Прикрепить изображение (png/jpg/webp)</translation>
    </message>
    <message>
        <source>Voice input (click to record)</source>
        <translation>Голосовой ввод (нажмите для записи)</translation>
    </message>
    <message>
        <source>Select images</source>
        <translation>Выберите изображения</translation>
    </message>
    <message>
        <source>Images (*.png *.jpg *.jpeg *.webp *.bmp);;All files (*.*)</source>
        <translation>Изображения (*.png *.jpg *.jpeg *.webp *.bmp);;Все файлы (*.*)</translation>
    </message>
    <message>
        <source>Choose a default template</source>
        <translation>Выберите шаблон по умолчанию</translation>
    </message>
    <message>
        <source>Simple chat</source>
        <translation>Простой чат</translation>
    </message>
    <message>
        <source>No models found at %1.</source>
        <translation>На %1 не найдено ни одной модели.</translation>
    </message>
    <message>
        <source>Start Ollama and download a model, for example &lt;code&gt;ollama pull qwen2.5-coder:7b&lt;/code&gt;, or choose another server in Settings &amp;rarr; Model. Then press the refresh button next to the model list.</source>
        <translation>Запустите Ollama и скачайте модель, например &lt;code&gt;ollama pull qwen2.5-coder:7b&lt;/code&gt;, или выберите другой сервер в разделе Настройки &amp;rarr; Модель. Затем нажмите кнопку обновления рядом со списком моделей.</translation>
    </message>
    <message>
        <source>Describing the attached images...</source>
        <translation>Описываю прикреплённые изображения...</translation>
    </message>
    <message>
        <source>You: %1</source>
        <translation>Вы: %1</translation>
    </message>
    <message>
        <source>Assistant:</source>
        <translation>Ассистент:</translation>
    </message>
    <message>
        <source>Remove</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <source>Recording... (click to stop)</source>
        <translation>Идёт запись... (нажмите для остановки)</translation>
    </message>
    <message>
        <source>Speech recognition error: %1</source>
        <translation>Ошибка распознавания речи: %1</translation>
    </message>
</context>
<context>
    <name>CommandConfirmDialog</name>
    <message>
        <source>Confirm commands</source>
        <translation>Подтверждение команд</translation>
    </message>
    <message>
        <source>The model wants to run these commands. Check the ones to run. Dangerous commands are not selected by default.</source>
        <translation>Модель хочет выполнить эти команды. Отметьте те, которые нужно выполнить. Опасные команды по умолчанию не выбраны.</translation>
    </message>
    <message>
        <source>Command</source>
        <translation>Команда</translation>
    </message>
    <message>
        <source>Risk</source>
        <translation>Риск</translation>
    </message>
    <message>
        <source>What it will do</source>
        <translation>Что будет сделано</translation>
    </message>
    <message>
        <source>Model's description: %1</source>
        <translation>Описание от модели: %1</translation>
    </message>
    <message>
        <source>Run selected</source>
        <translation>Выполнить выбранные</translation>
    </message>
</context>
<context>
    <name>CommandPolicy</name>
    <message numerus="yes">
        <source>%n line(s)</source>
        <translation><numerusform>строк: %n</numerusform><numerusform>строк: %n</numerusform><numerusform>строк: %n</numerusform></translation>
    </message>
    <message>
        <source>read-only</source>
        <translation>только чтение</translation>
    </message>
    <message>
        <source>changes files</source>
        <translation>изменяет файлы</translation>
    </message>
    <message>
        <source>dangerous</source>
        <translation>опасно</translation>
    </message>
    <message>
        <source>%1 is not allowed in this step. Allowed: %2.</source>
        <translation>Команда %1 не разрешена на этом шаге. Разрешены: %2.</translation>
    </message>
    <message>
        <source>Empty command.</source>
        <translation>Пустая команда.</translation>
    </message>
    <message>
        <source>in %1</source>
        <translation>в %1</translation>
    </message>
    <message>
        <source>python %1</source>
        <translation>python %1</translation>
    </message>
    <message>
        <source>Python code, %1:
%2</source>
        <translation>Код Python, %1:
%2</translation>
    </message>
    <message>
        <source>%1 and everything in it</source>
        <translation>%1 со всем содержимым</translation>
    </message>
    <message>
        <source>%1 (only if empty)</source>
        <translation>%1 (только если папка пуста)</translation>
    </message>
    <message>
        <source>%1, lines %2–%3</source>
        <translation>%1, строки %2–%3</translation>
    </message>
    <message>
        <source>new text: %1</source>
        <translation>новый текст (%1)</translation>
    </message>
    <message>
        <source>%1, %2</source>
        <translation>%1, %2</translation>
    </message>
    <message>
        <source>patch, %1</source>
        <translation>патч, %1</translation>
    </message>
</context>
<context>
    <name>ContextGraphWidget</name>
    <message>
        <source>Actions: %1</source>
        <translation>Действий: %1</translation>
    </message>
    <message>
        <source>Milestone</source>
        <translation>Веха</translation>
    </message>
    <message>
        <source>Context graph is empty.</source>
        <translation>Граф контекста пуст.</translation>
    </message>
</context>
<context>
    <name>ContextTreeView</name>
    <message>
        <source>Active in context</source>
        <translation>Активна в контексте</translation>
    </message>
    <message>
        <source>Invalidated: %1</source>
        <translation>Устарела: %1</translation>
    </message>
    <message>
        <source>Command was removed from context</source>
        <translation>Команда удалена из контекста</translation>
    </message>
    <message>
        <source>Result:</source>
        <translation>Результат:</translation>
    </message>
    <message>
        <source>... (click to expand)</source>
        <translation>... (нажмите, чтобы развернуть)</translation>
    </message>
    <message>
        <source>SEQUENCE: %1</source>
        <translation>ПОСЛЕДОВАТЕЛЬНОСТЬ: %1</translation>
    </message>
</context>
<context>
    <name>DevAssManager</name>
    <message>
        <source>No project is open.</source>
        <translation>Проект не открыт.</translation>
    </message>
    <message>
        <source>%1 is outside the project folder.</source>
        <translation>%1 находится вне папки проекта.</translation>
    </message>
</context>
<context>
    <name>DialogAgentHost</name>
    <message>
        <source>Answer the model's question</source>
        <translation>Ответьте на вопрос нейросети</translation>
    </message>
</context>
<context>
    <name>FasterWhisperRunner</name>
    <message>
        <source>Speech recognition: starting...</source>
        <translation>Распознавание речи: запуск...</translation>
    </message>
    <message>
        <source>Speech recognition: %1</source>
        <translation>Распознавание речи: %1</translation>
    </message>
    <message>
        <source>Speech recognition: scripts/run_faster_whisper_server.py was not found next to the program.</source>
        <translation>Распознавание речи: scripts/run_faster_whisper_server.py не найден рядом с программой.</translation>
    </message>
</context>
<context>
    <name>GraphWidget</name>
    <message>
        <source>Project Graph</source>
        <translation>Граф проекта</translation>
    </message>
    <message>
        <source>Open Project…</source>
        <translation>Открыть проект…</translation>
    </message>
    <message>
        <source>Select project folder</source>
        <translation>Выберите папку проекта</translation>
    </message>
</context>
<context>
    <name>IDEWidget</name>
    <message>
        <source>Outline</source>
        <translation>Структура</translation>
    </message>
    <message>
        <source>Graph</source>
        <translation>Граф</translation>
    </message>
    <message>
        <source>Error</source>
        <translation>Ошибка</translation>
    </message>
    <message>
        <source>Cannot open file</source>
        <translation>Не удалось открыть файл</translation>
    </message>
    <message>
        <source>Unsupported file type</source>
        <translation>Неподдерживаемый тип файла</translation>
    </message>
    <message>
        <source>Parsing failed</source>
        <translation>Ошибка разбора</translation>
    </message>
    <message>
        <source>Cannot write file</source>
        <translation>Не удалось записать файл</translation>
    </message>
</context>
<context>
    <name>LightRagClient</name>
    <message>
        <source>The server needs an API key (%1).</source>
        <translation>Серверу нужен API-ключ (%1).</translation>
    </message>
    <message>
        <source>Cannot open the file.</source>
        <translation>Не удалось открыть файл.</translation>
    </message>
    <message>
        <source>List documents</source>
        <translation>Список документов</translation>
    </message>
    <message>
        <source>Query</source>
        <translation>Запрос</translation>
    </message>
    <message>
        <source>Graph labels</source>
        <translation>Метки графа</translation>
    </message>
    <message>
        <source>Graph</source>
        <translation>Граф</translation>
    </message>
</context>
<context>
    <name>LightRagWidget</name>
    <message>
        <source>LightRAG server</source>
        <translation>Сервер LightRAG</translation>
    </message>
    <message>
        <source>API key, if the server has one</source>
        <translation>API-ключ, если он нужен серверу</translation>
    </message>
    <message>
        <source>Connect</source>
        <translation>Подключиться</translation>
    </message>
    <message>
        <source>Server</source>
        <translation>Сервер</translation>
    </message>
    <message>
        <source>LightRAG runs as a separate server (pip install "lightrag-hku[api]", then lightrag-server). It has its own LLM and embedding settings.</source>
        <translation>LightRAG работает как отдельный сервер (pip install "lightrag-hku[api]", затем lightrag-server). У него свои настройки LLM и эмбеддингов.</translation>
    </message>
    <message>
        <source>Documents</source>
        <translation>Документы</translation>
    </message>
    <message>
        <source>Ask</source>
        <translation>Задать вопрос</translation>
    </message>
    <message>
        <source>Knowledge graph</source>
        <translation>Граф знаний</translation>
    </message>
    <message>
        <source>Connected. %1</source>
        <translation>Подключено. %1</translation>
    </message>
    <message>
        <source>Cannot reach the server: %1</source>
        <translation>Сервер недоступен: %1</translation>
    </message>
    <message>
        <source>%1: %2</source>
        <translation>%1: %2</translation>
    </message>
    <message>
        <source>Upload finished. The server processes documents in the background; refresh to see their status.</source>
        <translation>Загрузка завершена. Сервер обрабатывает документы в фоне; обновите список, чтобы увидеть их статус.</translation>
    </message>
    <message>
        <source>Answered.</source>
        <translation>Ответ получен.</translation>
    </message>
    <message>
        <source>Upload files...</source>
        <translation>Загрузить файлы...</translation>
    </message>
    <message>
        <source>Upload folder...</source>
        <translation>Загрузить папку...</translation>
    </message>
    <message>
        <source>Refresh</source>
        <translation>Обновить</translation>
    </message>
    <message>
        <source>File</source>
        <translation>Файл</translation>
    </message>
    <message>
        <source>Status</source>
        <translation>Статус</translation>
    </message>
    <message>
        <source>Chunks</source>
        <translation>Фрагменты</translation>
    </message>
    <message>
        <source>Updated</source>
        <translation>Обновлено</translation>
    </message>
    <message>
        <source>Documents to upload</source>
        <translation>Документы для загрузки</translation>
    </message>
    <message>
        <source>Folder to upload</source>
        <translation>Папка для загрузки</translation>
    </message>
    <message>
        <source>mix: graph and vector search</source>
        <translation>mix: граф и векторный поиск</translation>
    </message>
    <message>
        <source>hybrid: local and global graph</source>
        <translation>hybrid: локальный и глобальный граф</translation>
    </message>
    <message>
        <source>local: entities near the question</source>
        <translation>local: сущности рядом с вопросом</translation>
    </message>
    <message>
        <source>global: relations across documents</source>
        <translation>global: связи между документами</translation>
    </message>
    <message>
        <source>naive: plain vector search</source>
        <translation>naive: простой векторный поиск</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>Режим</translation>
    </message>
    <message>
        <source>Question about the uploaded documents</source>
        <translation>Вопрос по загруженным документам</translation>
    </message>
    <message>
        <source>Waiting for the server...</source>
        <translation>Ожидание сервера...</translation>
    </message>
    <message>
        <source>Show</source>
        <translation>Показать</translation>
    </message>
    <message>
        <source>Entity</source>
        <translation>Сущность</translation>
    </message>
    <message>
        <source>Depth</source>
        <translation>Глубина</translation>
    </message>
    <message>
        <source>Connecting to %1...</source>
        <translation>Подключение к %1...</translation>
    </message>
    <message numerus="yes">
        <source>Uploading %n file(s)...</source>
        <translation><numerusform>Загрузка файлов: %n...</numerusform><numerusform>Загрузка файлов: %n...</numerusform><numerusform>Загрузка файлов: %n...</numerusform></translation>
    </message>
    <message>
        <source>The graph for this entity is empty.</source>
        <translation>Граф для этой сущности пуст.</translation>
    </message>
    <message>
        <source>%1 entities, %2 relations.</source>
        <translation>Сущностей: %1, связей: %2.</translation>
    </message>
</context>
<context>
    <name>LlmClient</name>
    <message>
        <source>No model is selected.</source>
        <translation>Модель не выбрана.</translation>
    </message>
    <message>
        <source>No answer within %1 s.</source>
        <translation>Нет ответа за %1 с.</translation>
    </message>
</context>
<context>
    <name>MainWindow</name>
    <message>
        <source>Undo last change</source>
        <translation>Отменить последнее изменение</translation>
    </message>
    <message>
        <source>Reverts the last change the assistant made to the project files (up to 50 steps back).</source>
        <translation>Отменяет последнее изменение файлов проекта, сделанное ассистентом (до 50 шагов назад).</translation>
    </message>
    <message>
        <source>Assistant</source>
        <translation>Ассистент</translation>
    </message>
    <message>
        <source>Settings</source>
        <translation>Настройки</translation>
    </message>
    <message>
        <source>Hide chat</source>
        <translation>Скрыть чат</translation>
    </message>
    <message>
        <source>Show chat</source>
        <translation>Показать чат</translation>
    </message>
    <message>
        <source>Hide log</source>
        <translation>Скрыть лог</translation>
    </message>
    <message>
        <source>Show log</source>
        <translation>Показать лог</translation>
    </message>
    <message>
        <source>Error</source>
        <translation>Ошибка</translation>
    </message>
    <message>
        <source>Please choose a template.</source>
        <translation>Необходимо выбрать шаблон.</translation>
    </message>
    <message>
        <source>Could not load the sequence.</source>
        <translation>Не удалось загрузить последовательность.</translation>
    </message>
    <message>
        <source>Provider: %1 | Model: %2</source>
        <translation>Провайдер: %1 | Модель: %2</translation>
    </message>
    <message>
        <source>&lt;not selected&gt;</source>
        <translation>&lt;не выбрана&gt;</translation>
    </message>
    <message>
        <source>Select project folder</source>
        <translation>Выберите папку проекта</translation>
    </message>
    <message>
        <source>Open LLM settings</source>
        <translation>Открыть настройки LLM</translation>
    </message>
    <message>
        <source>Hide LLM settings</source>
        <translation>Скрыть настройки LLM</translation>
    </message>
    <message>
        <source>Undo</source>
        <translation>Отменить</translation>
    </message>
    <message>
        <source>There is nothing to undo.</source>
        <translation>Отменять нечего.</translation>
    </message>
    <message>
        <source>The last change was undone.</source>
        <translation>Последнее изменение отменено.</translation>
    </message>
    <message>
        <source>Done</source>
        <translation>Готово</translation>
    </message>
    <message>
        <source>No files in the project need a description.</source>
        <translation>В проекте нет файлов, требующих описания.</translation>
    </message>
    <message>
        <source>Generating descriptions…</source>
        <translation>Генерация описаний…</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>Отмена</translation>
    </message>
    <message numerus="yes">
        <source>[Images attached: %n]</source>
        <translation><numerusform>[Прикреплено изображений: %n]</numerusform><numerusform>[Прикреплено изображений: %n]</numerusform><numerusform>[Прикреплено изображений: %n]</numerusform></translation>
    </message>
    <message>
        <source>User question</source>
        <translation>Вопрос пользователя</translation>
    </message>
</context>
<context>
    <name>MethodEditDialog</name>
    <message>
        <source>Type a message...</source>
        <translation>Введите сообщение...</translation>
    </message>
    <message>
        <source>Ask</source>
        <translation>Задать вопрос</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>Отмена</translation>
    </message>
    <message>
        <source>Save</source>
        <translation>Сохранить</translation>
    </message>
    <message>
        <source>Edit Block</source>
        <translation>Редактирование блока</translation>
    </message>
</context>
<context>
    <name>MicTranscriber</name>
    <message>
        <source>Set the whisper program and the model path first</source>
        <translation>Сначала укажите программу whisper и путь к модели</translation>
    </message>
    <message>
        <source>The requested audio format is not supported</source>
        <translation>Запрошенный аудиоформат не поддерживается</translation>
    </message>
    <message>
        <source>Failed to start audio input</source>
        <translation>Не удалось начать запись звука</translation>
    </message>
    <message>
        <source>Cannot open the WAV file for writing: %1</source>
        <translation>Не удалось открыть WAV-файл для записи: %1</translation>
    </message>
</context>
<context>
    <name>ModelReply</name>
    <message>
        <source>The request was cancelled.</source>
        <translation>Запрос отменён.</translation>
    </message>
    <message>
        <source>The model server returned an error.</source>
        <translation>Сервер модели вернул ошибку.</translation>
    </message>
</context>
<context>
    <name>ModelSettingsPanel</name>
    <message>
        <source>Generation and Connection</source>
        <translation>Генерация и подключение</translation>
    </message>
    <message>
        <source>Select provider (ollama/llama.cpp), endpoint and generation parameters. Changes are applied after pressing Save.</source>
        <translation>Выберите провайдера (ollama/llama.cpp), адрес сервера и параметры генерации. Изменения применяются после нажатия «Сохранить».</translation>
    </message>
    <message>
        <source>No limit (-1)</source>
        <translation>Без ограничения (-1)</translation>
    </message>
    <message>
        <source>One stop word per line</source>
        <translation>Одно стоп-слово на строку</translation>
    </message>
    <message>
        <source>Connection</source>
        <translation>Подключение</translation>
    </message>
    <message>
        <source>Provider</source>
        <translation>Провайдер</translation>
    </message>
    <message>
        <source>Endpoint</source>
        <translation>Адрес сервера</translation>
    </message>
    <message>
        <source>Defaults: ollama -&gt; http://127.0.0.1:11434, llama.cpp -&gt; http://127.0.0.1:8080</source>
        <translation>По умолчанию: ollama -&gt; http://127.0.0.1:11434, llama.cpp -&gt; http://127.0.0.1:8080</translation>
    </message>
    <message>
        <source>Sampling</source>
        <translation>Сэмплирование</translation>
    </message>
    <message>
        <source>Temperature</source>
        <translation>Температура</translation>
    </message>
    <message>
        <source>Top P</source>
        <translation>Top P</translation>
    </message>
    <message>
        <source>Top K</source>
        <translation>Top K</translation>
    </message>
    <message>
        <source>Penalties and repetition</source>
        <translation>Штрафы и повторы</translation>
    </message>
    <message>
        <source>Repeat penalty</source>
        <translation>Штраф за повторы</translation>
    </message>
    <message>
        <source>Presence penalty</source>
        <translation>Штраф за присутствие</translation>
    </message>
    <message>
        <source>Frequency penalty</source>
        <translation>Штраф за частоту</translation>
    </message>
    <message>
        <source>Repeat last N</source>
        <translation>Окно повторов (последние N)</translation>
    </message>
    <message>
        <source>Temperature decay</source>
        <translation>Затухание температуры</translation>
    </message>
    <message>
        <source>Mirostat</source>
        <translation>Mirostat</translation>
    </message>
    <message>
        <source>Mode (0/1/2)</source>
        <translation>Режим (0/1/2)</translation>
    </message>
    <message>
        <source>Tau</source>
        <translation>Tau</translation>
    </message>
    <message>
        <source>Eta</source>
        <translation>Eta</translation>
    </message>
    <message>
        <source>Generation</source>
        <translation>Генерация</translation>
    </message>
    <message>
        <source>Limits</source>
        <translation>Ограничения</translation>
    </message>
    <message>
        <source>Num context (num_ctx)</source>
        <translation>Размер контекста (num_ctx)</translation>
    </message>
    <message>
        <source>Num predict (num_predict)</source>
        <translation>Макс. длина ответа (num_predict)</translation>
    </message>
    <message>
        <source>Seed</source>
        <translation>Seed</translation>
    </message>
    <message>
        <source>Reasoning effort</source>
        <translation>Глубина рассуждений</translation>
    </message>
    <message>
        <source>Stop words</source>
        <translation>Стоп-слова</translation>
    </message>
    <message>
        <source>Context and output</source>
        <translation>Контекст и вывод</translation>
    </message>
    <message>
        <source>Reset</source>
        <translation>Сбросить</translation>
    </message>
    <message>
        <source>Save</source>
        <translation>Сохранить</translation>
    </message>
</context>
<context>
    <name>OmniAsrRunner</name>
    <message>
        <source>Omni ASR: scripts/run_omni_asr.py was not found next to the program.</source>
        <translation>Omni ASR: scripts/run_omni_asr.py не найден рядом с программой.</translation>
    </message>
    <message>
        <source>Omni ASR: %1</source>
        <translation>Omni ASR: %1</translation>
    </message>
    <message>
        <source>Omni ASR needs Python packages: pip install torch torchaudio transformers soundfile</source>
        <translation>Для Omni ASR нужны пакеты Python: pip install torch torchaudio transformers soundfile</translation>
    </message>
</context>
<context>
    <name>PathGuard</name>
    <message>
        <source>No project is open.</source>
        <translation>Проект не открыт.</translation>
    </message>
    <message>
        <source>The path is empty.</source>
        <translation>Путь не указан.</translation>
    </message>
    <message>
        <source>%1 is outside the project folder %2.</source>
        <translation>%1 находится вне папки проекта %2.</translation>
    </message>
</context>
<context>
    <name>ProjectIndexer</name>
    <message>
        <source>Indexing the project with tree-sitter...</source>
        <translation>Индексирую проект с помощью tree-sitter...</translation>
    </message>
</context>
<context>
    <name>ProjectRelationView</name>
    <message>
        <source>Graph</source>
        <translation>Граф</translation>
    </message>
    <message>
        <source>Rebuild</source>
        <translation>Перестроить</translation>
    </message>
    <message>
        <source>Validate</source>
        <translation>Проверить</translation>
    </message>
    <message>
        <source>Auto-Layout</source>
        <translation>Авторасстановка</translation>
    </message>
    <message>
        <source>Open in Editor</source>
        <translation>Открыть в редакторе</translation>
    </message>
    <message>
        <source>Rename Method...</source>
        <translation>Переименовать метод...</translation>
    </message>
    <message>
        <source>Restore File from Stash</source>
        <translation>Откатить файл из stash</translation>
    </message>
    <message>
        <source>Rename</source>
        <translation>Переименование</translation>
    </message>
    <message>
        <source>New method name:</source>
        <translation>Новое имя метода:</translation>
    </message>
    <message>
        <source>Could not rename the method.</source>
        <translation>Не удалось переименовать метод.</translation>
    </message>
    <message>
        <source>Add Method Stub...</source>
        <translation>Добавить заглушку метода...</translation>
    </message>
    <message>
        <source>Restore Class File from Stash</source>
        <translation>Откатить файл класса из stash</translation>
    </message>
    <message>
        <source>New Method</source>
        <translation>Новый метод</translation>
    </message>
    <message>
        <source>Return type:</source>
        <translation>Тип возврата:</translation>
    </message>
    <message>
        <source>Method name:</source>
        <translation>Имя метода:</translation>
    </message>
    <message>
        <source>Could not add the method.</source>
        <translation>Не удалось добавить метод.</translation>
    </message>
</context>
<context>
    <name>PromptResolver</name>
    <message>
        <source>Project root folder</source>
        <translation>Корневая папка проекта</translation>
    </message>
    <message>
        <source>Project file list</source>
        <translation>Список файлов проекта</translation>
    </message>
    <message>
        <source>Last 12 chat messages</source>
        <translation>Последние 12 сообщений из чата</translation>
    </message>
    <message>
        <source>Last 100 user messages from the chat</source>
        <translation>Последние 100 сообщений пользователя из чата</translation>
    </message>
    <message>
        <source>Contents of files opened with OPEN_FILE</source>
        <translation>Содержимое файлов, открытых командой OPEN_FILE</translation>
    </message>
    <message>
        <source>Commands available in this step</source>
        <translation>Список команд, доступных на этом шаге</translation>
    </message>
    <message>
        <source>Current user request</source>
        <translation>Текущий запрос пользователя</translation>
    </message>
    <message>
        <source>Current date (YYYY-MM-DD)</source>
        <translation>Текущая дата (YYYY-MM-DD)</translation>
    </message>
    <message>
        <source>Same as the current date (YYYY-MM-DD)</source>
        <translation>Синоним текущей даты (YYYY-MM-DD)</translation>
    </message>
    <message>
        <source>Current date/time with UTC offset</source>
        <translation>Текущие дата и время со смещением от UTC</translation>
    </message>
    <message>
        <source>Last assistant response</source>
        <translation>Последний ответ ассистента</translation>
    </message>
    <message>
        <source>Add project file descriptions</source>
        <translation>Добавить описания файлов проекта</translation>
    </message>
    <message>
        <source>What was already done in the previous steps</source>
        <translation>Что уже сделано на предыдущих этапах</translation>
    </message>
    <message>
        <source>Token count of the current context</source>
        <translation>Число токенов текущего контекста</translation>
    </message>
    <message>
        <source>Short excerpts from SEARCH_WEB / SEARCH_PROJECT</source>
        <translation>Краткие выдержки из SEARCH_WEB / SEARCH_PROJECT</translation>
    </message>
    <message>
        <source>Sequences available in the template, as: number. description; sequenceName = "file_name_without_.seq.json"; summarize = true;</source>
        <translation>Список доступных последовательностей в шаблоне в формате: номер. описание; sequenceName = "имя_файла_без_.seq.json"; summarize = true;</translation>
    </message>
    <message>
        <source>Context chain of the executed commands with a visual status</source>
        <translation>Цепочка контекста из выполненных команд с визуальным статусом</translation>
    </message>
    <message>
        <source>Tree view of the project folder structure</source>
        <translation>Древовидное представление структуры папок проекта</translation>
    </message>
</context>
<context>
    <name>PromptSequenceEditor</name>
    <message>
        <source>Sequence name</source>
        <translation>Название последовательности</translation>
    </message>
    <message>
        <source>Name:</source>
        <translation>Имя:</translation>
    </message>
    <message>
        <source>Description:</source>
        <translation>Описание:</translation>
    </message>
    <message>
        <source>Steps:</source>
        <translation>Шаги:</translation>
    </message>
    <message>
        <source>Add</source>
        <translation>Добавить</translation>
    </message>
    <message>
        <source>Edit</source>
        <translation>Редактировать</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <source>Up</source>
        <translation>Вверх</translation>
    </message>
    <message>
        <source>Down</source>
        <translation>Вниз</translation>
    </message>
    <message>
        <source>Step %1</source>
        <translation>Шаг %1</translation>
    </message>
</context>
<context>
    <name>PromptStepDialog</name>
    <message>
        <source>Edit Step</source>
        <translation>Редактирование шага</translation>
    </message>
    <message>
        <source>Step template</source>
        <translation>Шаблон шага</translation>
    </message>
    <message>
        <source>Insert variables into the template (format: {{TOKEN}}).</source>
        <translation>Вставка переменных в шаблон (формат: {{TOKEN}}).</translation>
    </message>
    <message>
        <source>Filter inserts…</source>
        <translation>Фильтр вставок…</translation>
    </message>
    <message>
        <source>Insert</source>
        <translation>Вставить</translation>
    </message>
    <message>
        <source>OPEN_SEQUENCE_INFO — transitions/links</source>
        <translation>OPEN_SEQUENCE_INFO — переходы/связи</translation>
    </message>
    <message>
        <source>Inserts</source>
        <translation>Вставки</translation>
    </message>
    <message>
        <source>Commands allowed in this step (listed in {{SUPPORTED_COMMANDS}}).</source>
        <translation>Разрешённые команды для этого шага (попадают в {{SUPPORTED_COMMANDS}}).</translation>
    </message>
    <message>
        <source>Filter commands…</source>
        <translation>Фильтр команд…</translation>
    </message>
    <message>
        <source>Select All</source>
        <translation>Выбрать все</translation>
    </message>
    <message>
        <source>Clear All</source>
        <translation>Снять все</translation>
    </message>
    <message>
        <source>Commands</source>
        <translation>Команды</translation>
    </message>
    <message>
        <source>Sequences available in the template (links for {{OPEN_SEQUENCE_INFO}}).</source>
        <translation>Доступные последовательности в шаблоне (связи для {{OPEN_SEQUENCE_INFO}}).</translation>
    </message>
    <message>
        <source>Filter sequences…</source>
        <translation>Фильтр последовательностей…</translation>
    </message>
    <message>
        <source>Transitions</source>
        <translation>Переходы</translation>
    </message>
    <message>
        <source>Step Options</source>
        <translation>Параметры шага</translation>
    </message>
    <message>
        <source>Loop step (repeat until stopped)</source>
        <translation>Зациклить шаг (повторять до остановки)</translation>
    </message>
    <message>
        <source>Summarize before transition (recap of completed work)</source>
        <translation>Сделать саммаризацию перед переходом (резюме выполненной работы)</translation>
    </message>
    <message>
        <source>Insert {{OPEN_SEQUENCE_INFO}}</source>
        <translation>Вставить {{OPEN_SEQUENCE_INFO}}</translation>
    </message>
    <message>
        <source>Sequences available in the template:</source>
        <translation>Доступные последовательности в шаблоне:</translation>
    </message>
</context>
<context>
    <name>PythonScripts</name>
    <message>
        <source>Python was not found. Install Python 3 and add it to PATH, or set its path in Settings.</source>
        <translation>Python не найден. Установите Python 3 и добавьте его в PATH или укажите путь к нему в настройках.</translation>
    </message>
    <message>
        <source>Python did not run (exit code %1). On Windows this is often the Microsoft Store placeholder: install Python from python.org.</source>
        <translation>Python не запустился (код выхода %1). В Windows это часто заглушка из Microsoft Store: установите Python с python.org.</translation>
    </message>
</context>
<context>
    <name>Rag</name>
    <message>
        <source>No embedding model is set.</source>
        <translation>Модель эмбеддингов не задана.</translation>
    </message>
    <message>
        <source>The embedding request timed out.</source>
        <translation>Истекло время ожидания запроса эмбеддингов.</translation>
    </message>
    <message>
        <source>Embedding request failed: %1</source>
        <translation>Ошибка запроса эмбеддингов: %1</translation>
    </message>
    <message>
        <source>The server returned %1 embeddings for %2 texts. Is %3 an embedding model?</source>
        <translation>Сервер вернул %1 эмбеддингов для %2 текстов. Точно ли %3 — модель эмбеддингов?</translation>
    </message>
    <message>
        <source>Cannot open the index: %1</source>
        <translation>Не удалось открыть индекс: %1</translation>
    </message>
    <message>
        <source>Cannot write the index: %1</source>
        <translation>Не удалось записать индекс: %1</translation>
    </message>
    <message>
        <source>Stopped. Indexed %1 files (%2 chunks).</source>
        <translation>Остановлено. Проиндексировано файлов: %1 (фрагментов: %2).</translation>
    </message>
    <message>
        <source>Indexed %1 files (%2 chunks), %3 unchanged, %4 skipped, %5 removed. The index has %6 files and %7 chunks.</source>
        <translation>Проиндексировано файлов: %1 (фрагментов: %2), без изменений: %3, пропущено: %4, удалено: %5. В индексе файлов: %6, фрагментов: %7.</translation>
    </message>
    <message>
        <source>These documents are not indexed yet. Open Settings &gt; Documents and index the project.</source>
        <translation>Эти документы ещё не проиндексированы. Откройте «Настройки &gt; Документы» и проиндексируйте проект.</translation>
    </message>
    <message>
        <source>Nothing relevant was found in the index.</source>
        <translation>В индексе не найдено ничего подходящего.</translation>
    </message>
    <message>
        <source>Select a chat model first.</source>
        <translation>Сначала выберите модель чата.</translation>
    </message>
    <message>
        <source>PDF needs the pdftotext tool (Poppler) on PATH.</source>
        <translation>Для PDF нужна утилита pdftotext (Poppler) в PATH.</translation>
    </message>
    <message>
        <source>pdftotext could not read %1.</source>
        <translation>pdftotext не смог прочитать %1.</translation>
    </message>
    <message>
        <source>Cannot open %1.</source>
        <translation>Не удалось открыть %1.</translation>
    </message>
    <message>
        <source>%1 is not a text file.</source>
        <translation>%1 не является текстовым файлом.</translation>
    </message>
</context>
<context>
    <name>Rag::RagWidget</name>
    <message>
        <source>Documents: search and questions</source>
        <translation>Документы: поиск и вопросы</translation>
    </message>
    <message>
        <source>Browse...</source>
        <translation>Обзор...</translation>
    </message>
    <message>
        <source>Folder</source>
        <translation>Папка</translation>
    </message>
    <message>
        <source>An embedding model on the same server as the chat model, for example nomic-embed-text or bge-m3 (ollama pull nomic-embed-text).</source>
        <translation>Модель эмбеддингов на том же сервере, что и модель чата, например nomic-embed-text или bge-m3 (ollama pull nomic-embed-text).</translation>
    </message>
    <message>
        <source>Embedding model</source>
        <translation>Модель эмбеддингов</translation>
    </message>
    <message>
        <source>Excerpts per answer</source>
        <translation>Фрагментов на ответ</translation>
    </message>
    <message>
        <source>Index</source>
        <translation>Индексировать</translation>
    </message>
    <message>
        <source>Reads the folder's text, code and PDF files. Only changed files are processed again.</source>
        <translation>Читает из папки текстовые файлы, код и PDF. Повторно обрабатываются только изменённые файлы.</translation>
    </message>
    <message>
        <source>Stop</source>
        <translation>Остановить</translation>
    </message>
    <message>
        <source>Ask about these documents</source>
        <translation>Вопрос по этим документам</translation>
    </message>
    <message>
        <source>Ask</source>
        <translation>Задать вопрос</translation>
    </message>
    <message>
        <source>Double-click to open the file</source>
        <translation>Дважды щёлкните, чтобы открыть файл</translation>
    </message>
    <message>
        <source>Folder to index</source>
        <translation>Папка для индексации</translation>
    </message>
    <message>
        <source>Indexing %1 (%2 of %3)</source>
        <translation>Индексация %1 (%2 из %3)</translation>
    </message>
    <message>
        <source>Choose a folder to index.</source>
        <translation>Выберите папку для индексации.</translation>
    </message>
    <message>
        <source>This folder has an index. Index again to pick up changes, or ask a question.</source>
        <translation>Для этой папки уже есть индекс. Проиндексируйте её снова, чтобы учесть изменения, или задайте вопрос.</translation>
    </message>
    <message>
        <source>This folder is not indexed yet. Make sure the embedding model is downloaded, then press Index.</source>
        <translation>Эта папка ещё не проиндексирована. Убедитесь, что модель эмбеддингов загружена, и нажмите «Индексировать».</translation>
    </message>
    <message>
        <source>The folder does not exist.</source>
        <translation>Папка не существует.</translation>
    </message>
    <message>
        <source>Looking for files...</source>
        <translation>Поиск файлов...</translation>
    </message>
    <message>
        <source>Searching and asking the model...</source>
        <translation>Поиск и запрос к модели...</translation>
    </message>
    <message numerus="yes">
        <source>Answer from %n excerpt(s).</source>
        <translation><numerusform>Ответ по фрагментам: %n.</numerusform><numerusform>Ответ по фрагментам: %n.</numerusform><numerusform>Ответ по фрагментам: %n.</numerusform></translation>
    </message>
</context>
<context>
    <name>ResultDisplayWidget</name>
    <message>
        <source>Ask about the project in the field below. The answers, and the results of the commands the assistant runs, appear here.</source>
        <translation>Задайте вопрос о проекте в поле ниже. Здесь появятся ответы и результаты команд, которые выполняет ассистент.</translation>
    </message>
    <message>
        <source>Show reasoning</source>
        <translation>Показать рассуждение</translation>
    </message>
    <message>
        <source>&lt;i&gt;No content&lt;/i&gt;</source>
        <translation>&lt;i&gt;Нет содержимого&lt;/i&gt;</translation>
    </message>
    <message>
        <source>Hide reasoning</source>
        <translation>Скрыть рассуждение</translation>
    </message>
    <message>
        <source>Show commands and sequences</source>
        <translation>Показать команды и последовательности</translation>
    </message>
    <message>
        <source>Time</source>
        <translation>Время</translation>
    </message>
    <message>
        <source>Description</source>
        <translation>Описание</translation>
    </message>
    <message>
        <source>Hide commands and sequences</source>
        <translation>Скрыть команды и последовательности</translation>
    </message>
    <message>
        <source>Copy question</source>
        <translation>Копировать вопрос</translation>
    </message>
    <message>
        <source>Copy answer</source>
        <translation>Копировать ответ</translation>
    </message>
    <message>
        <source>Copy reasoning</source>
        <translation>Копировать рассуждение</translation>
    </message>
    <message>
        <source>Unnamed sequence</source>
        <translation>Последовательность без имени</translation>
    </message>
    <message>
        <source>No commands applied</source>
        <translation>Нет применённых команд</translation>
    </message>
    <message>
        <source>Result</source>
        <translation>Результат</translation>
    </message>
</context>
<context>
    <name>RunTemplateOrSequenceDialog</name>
    <message>
        <source>Select Sequence or Template</source>
        <translation>Выбор последовательности или шаблона</translation>
    </message>
    <message>
        <source>Mode:</source>
        <translation>Режим:</translation>
    </message>
    <message>
        <source>Run Individual Sequence</source>
        <translation>Запустить отдельную последовательность</translation>
    </message>
    <message>
        <source>Run Template</source>
        <translation>Запустить шаблон</translation>
    </message>
    <message>
        <source>Available Sequences:</source>
        <translation>Доступные последовательности:</translation>
    </message>
    <message>
        <source>Sequences</source>
        <translation>Последовательности</translation>
    </message>
    <message>
        <source>Solution Templates:</source>
        <translation>Шаблоны решений:</translation>
    </message>
    <message>
        <source>Template Details</source>
        <translation>Сведения о шаблоне</translation>
    </message>
    <message>
        <source>Default Sequence:</source>
        <translation>Последовательность по умолчанию:</translation>
    </message>
    <message>
        <source>Templates</source>
        <translation>Шаблоны</translation>
    </message>
    <message>
        <source>Image not found</source>
        <translation>Изображение не найдено</translation>
    </message>
    <message>
        <source>Selection Error</source>
        <translation>Ошибка выбора</translation>
    </message>
    <message>
        <source>Please select a sequence.</source>
        <translation>Выберите последовательность.</translation>
    </message>
    <message>
        <source>Please select a template.</source>
        <translation>Выберите шаблон.</translation>
    </message>
    <message>
        <source>Please select a default sequence for the template.</source>
        <translation>Выберите последовательность по умолчанию для шаблона.</translation>
    </message>
</context>
<context>
    <name>SequenceEditorController</name>
    <message>
        <source>Error</source>
        <translation>Ошибка</translation>
    </message>
    <message>
        <source>Sequence not found: %1</source>
        <translation>Последовательность не найдена: %1</translation>
    </message>
    <message>
        <source>Sequence editor: %1</source>
        <translation>Редактор последовательности: %1</translation>
    </message>
    <message>
        <source>Save</source>
        <translation>Сохранить</translation>
    </message>
    <message>
        <source>Close</source>
        <translation>Закрыть</translation>
    </message>
    <message>
        <source>Saved</source>
        <translation>Сохранено</translation>
    </message>
    <message>
        <source>The sequence has been saved.</source>
        <translation>Последовательность сохранена.</translation>
    </message>
    <message>
        <source>Could not save the sequence.</source>
        <translation>Не удалось сохранить последовательность.</translation>
    </message>
    <message>
        <source>No sequences</source>
        <translation>Нет последовательностей</translation>
    </message>
    <message>
        <source>The folder has no *.seq.json files.</source>
        <translation>В папке нет ни одного файла *.seq.json.</translation>
    </message>
    <message>
        <source>Choose a sequence</source>
        <translation>Выберите последовательность</translation>
    </message>
    <message>
        <source>File:</source>
        <translation>Файл:</translation>
    </message>
    <message>
        <source>New sequence</source>
        <translation>Новая последовательность</translation>
    </message>
    <message>
        <source>Name:</source>
        <translation>Имя:</translation>
    </message>
    <message>
        <source>Create Sequence</source>
        <translation>Создание последовательности</translation>
    </message>
    <message>
        <source>Where would you like to create the sequence?</source>
        <translation>Где создать последовательность?</translation>
    </message>
    <message>
        <source>In PromptSequences folder</source>
        <translation>В папке PromptSequences</translation>
    </message>
    <message>
        <source>In Template folder</source>
        <translation>В папке шаблона</translation>
    </message>
    <message>
        <source>Cannot save sequence to: %1</source>
        <translation>Не удалось сохранить последовательность в файл: %1</translation>
    </message>
    <message>
        <source>OK</source>
        <translation>OK</translation>
    </message>
    <message>
        <source>Sequence created:
%1</source>
        <translation>Последовательность создана:
%1</translation>
    </message>
    <message>
        <source>Select Template Folder</source>
        <translation>Выберите папку шаблона</translation>
    </message>
    <message>
        <source>Invalid Template Directory</source>
        <translation>Неверная папка шаблона</translation>
    </message>
    <message>
        <source>The selected directory is not a valid template directory (missing template.templ.json).</source>
        <translation>Выбранная папка не является папкой шаблона (нет файла template.templ.json).</translation>
    </message>
    <message>
        <source>Cannot create sequence in template directory: %1</source>
        <translation>Не удалось создать последовательность в папке шаблона: %1</translation>
    </message>
    <message>
        <source>Sequence created in template:
%1</source>
        <translation>Последовательность создана в шаблоне:
%1</translation>
    </message>
    <message>
        <source>Solution Templates</source>
        <translation>Шаблоны решений</translation>
    </message>
    <message>
        <source>No sequences found.</source>
        <translation>Последовательности не найдены.</translation>
    </message>
    <message>
        <source>Export</source>
        <translation>Экспорт</translation>
    </message>
    <message>
        <source>Choose:</source>
        <translation>Выберите:</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Имя</translation>
    </message>
    <message>
        <source>Template name:</source>
        <translation>Имя шаблона:</translation>
    </message>
    <message>
        <source>Description</source>
        <translation>Описание</translation>
    </message>
    <message>
        <source>Description:</source>
        <translation>Описание:</translation>
    </message>
    <message>
        <source>Failed</source>
        <translation>Ошибка</translation>
    </message>
    <message>
        <source>A template with this name already exists.</source>
        <translation>Шаблон с таким именем уже существует.</translation>
    </message>
    <message>
        <source>Export failed.</source>
        <translation>Не удалось экспортировать.</translation>
    </message>
    <message>
        <source>Exported.</source>
        <translation>Экспортировано.</translation>
    </message>
</context>
<context>
    <name>SequenceGraphWidget</name>
    <message>
        <source>Sequence Relationships Graph</source>
        <translation>Граф связей последовательностей</translation>
    </message>
    <message>
        <source>Template:</source>
        <translation>Шаблон:</translation>
    </message>
    <message>
        <source>Fit All</source>
        <translation>Вписать</translation>
    </message>
    <message>
        <source>Fit the whole graph into the view</source>
        <translation>Вписать весь граф в окно</translation>
    </message>
    <message>
        <source>All Sequences</source>
        <translation>Все последовательности</translation>
    </message>
</context>
<context>
    <name>SequenceRelationView</name>
    <message>
        <source>Link Not Created</source>
        <translation>Связь не создана</translation>
    </message>
    <message>
        <source>Could not create a link between %1 and %2.</source>
        <translation>Не удалось создать связь между %1 и %2.</translation>
    </message>
    <message>
        <source>Fit All</source>
        <translation>Вписать</translation>
    </message>
    <message>
        <source>Show Selection</source>
        <translation>Показать выделенное</translation>
    </message>
    <message>
        <source>Reset Zoom</source>
        <translation>Сбросить масштаб</translation>
    </message>
    <message>
        <source>Refresh Graph</source>
        <translation>Обновить граф</translation>
    </message>
    <message>
        <source>Create Sequence Here</source>
        <translation>Создать последовательность здесь</translation>
    </message>
    <message>
        <source>Open/Edit</source>
        <translation>Открыть/редактировать</translation>
    </message>
    <message>
        <source>Start Link Here</source>
        <translation>Начать связь отсюда</translation>
    </message>
    <message>
        <source>Remove from Template</source>
        <translation>Удалить из шаблона</translation>
    </message>
    <message>
        <source>New Sequence</source>
        <translation>Новая последовательность</translation>
    </message>
    <message>
        <source>Sequence name:</source>
        <translation>Название последовательности:</translation>
    </message>
    <message>
        <source>Not Created</source>
        <translation>Не создано</translation>
    </message>
    <message>
        <source>Could not create the sequence.</source>
        <translation>Не удалось создать последовательность.</translation>
    </message>
    <message>
        <source>Delete Sequence</source>
        <translation>Удалить последовательность</translation>
    </message>
    <message>
        <source>Remove "%1" from the template?</source>
        <translation>Удалить "%1" из шаблона?</translation>
    </message>
    <message>
        <source>Delete Link</source>
        <translation>Удалить связь</translation>
    </message>
</context>
<context>
    <name>SolutionTemplateEditor</name>
    <message>
        <source>Template Information</source>
        <translation>Информация о шаблоне</translation>
    </message>
    <message>
        <source>Name:</source>
        <translation>Имя:</translation>
    </message>
    <message>
        <source>Description:</source>
        <translation>Описание:</translation>
    </message>
    <message>
        <source>Icon:</source>
        <translation>Значок:</translation>
    </message>
    <message>
        <source>No image selected</source>
        <translation>Изображение не выбрано</translation>
    </message>
    <message>
        <source>Select Image...</source>
        <translation>Выбрать изображение...</translation>
    </message>
    <message>
        <source>Sequences in Template</source>
        <translation>Последовательности в шаблоне</translation>
    </message>
    <message>
        <source>Available Sequences</source>
        <translation>Доступные последовательности</translation>
    </message>
    <message>
        <source>-&gt; Add to Template</source>
        <translation>-&gt; Добавить в шаблон</translation>
    </message>
    <message>
        <source>Remove from Template &lt;-</source>
        <translation>Убрать из шаблона &lt;-</translation>
    </message>
    <message>
        <source>Set as Main</source>
        <translation>Сделать основной</translation>
    </message>
    <message>
        <source>Template Sequences</source>
        <translation>Последовательности шаблона</translation>
    </message>
    <message>
        <source>Basic Info</source>
        <translation>Основное</translation>
    </message>
    <message>
        <source>Sequence Graph</source>
        <translation>Граф последовательностей</translation>
    </message>
    <message>
        <source>Image not found</source>
        <translation>Изображение не найдено</translation>
    </message>
    <message>
        <source>No Selection</source>
        <translation>Ничего не выбрано</translation>
    </message>
    <message>
        <source>Please select a sequence to add.</source>
        <translation>Выберите последовательность для добавления.</translation>
    </message>
    <message>
        <source>Create Directory</source>
        <translation>Создание папки</translation>
    </message>
    <message>
        <source>Could not create template directory: %1</source>
        <translation>Не удалось создать папку шаблона: %1</translation>
    </message>
    <message>
        <source>Add Sequence</source>
        <translation>Добавление последовательности</translation>
    </message>
    <message>
        <source>Sequence already exists in template directory: %1</source>
        <translation>Последовательность уже есть в папке шаблона: %1</translation>
    </message>
    <message>
        <source>Failed to copy sequence to template directory: %1</source>
        <translation>Не удалось скопировать последовательность в папку шаблона: %1</translation>
    </message>
    <message>
        <source>Please select a sequence to remove.</source>
        <translation>Выберите последовательность для удаления.</translation>
    </message>
    <message>
        <source>Please select a sequence to set as main.</source>
        <translation>Выберите последовательность, которую нужно сделать основной.</translation>
    </message>
    <message>
        <source>Select Image</source>
        <translation>Выбор изображения</translation>
    </message>
    <message>
        <source>Image Files (*.png *.jpg *.jpeg *.bmp *.svg *.gif)</source>
        <translation>Изображения (*.png *.jpg *.jpeg *.bmp *.svg *.gif)</translation>
    </message>
    <message>
        <source>Could not load image</source>
        <translation>Не удалось загрузить изображение</translation>
    </message>
    <message>
        <source>Template name is not set. Please set a template name first.</source>
        <translation>Имя шаблона не задано. Сначала укажите имя шаблона.</translation>
    </message>
    <message>
        <source>Could not create images directory: %1</source>
        <translation>Не удалось создать папку изображений: %1</translation>
    </message>
    <message>
        <source>Failed to copy image to template directory: %1</source>
        <translation>Не удалось скопировать изображение в папку шаблона: %1</translation>
    </message>
</context>
<context>
    <name>SolutionTemplateViewer</name>
    <message>
        <source>Run Template</source>
        <translation>Запустить шаблон</translation>
    </message>
    <message>
        <source>Would run template with sequence: %1</source>
        <translation>Шаблон будет запущен с последовательностью: %1</translation>
    </message>
    <message>
        <source>No Sequence</source>
        <translation>Нет последовательности</translation>
    </message>
    <message>
        <source>No sequence selected for this template.</source>
        <translation>Для этого шаблона не выбрана последовательность.</translation>
    </message>
    <message>
        <source>Solution Templates</source>
        <translation>Шаблоны решений</translation>
    </message>
    <message>
        <source>Add Template</source>
        <translation>Добавить шаблон</translation>
    </message>
    <message>
        <source>Edit Template</source>
        <translation>Редактировать шаблон</translation>
    </message>
    <message>
        <source>Delete Template</source>
        <translation>Удалить шаблон</translation>
    </message>
    <message>
        <source>Export Template</source>
        <translation>Экспортировать шаблон</translation>
    </message>
    <message>
        <source>Import Sequence as Template</source>
        <translation>Импортировать последовательность как шаблон</translation>
    </message>
    <message>
        <source>Template Details</source>
        <translation>Сведения о шаблоне</translation>
    </message>
    <message>
        <source>No template selected</source>
        <translation>Шаблон не выбран</translation>
    </message>
    <message>
        <source>Default Sequence:</source>
        <translation>Последовательность по умолчанию:</translation>
    </message>
    <message>
        <source>Please select a template to edit.</source>
        <translation>Выберите шаблон для редактирования.</translation>
    </message>
    <message>
        <source>Edit Solution Template</source>
        <translation>Редактирование шаблона решения</translation>
    </message>
    <message>
        <source>Sequence manager is not available. Template editing requires a valid sequence manager.</source>
        <translation>Менеджер последовательностей недоступен. Для редактирования шаблона нужен рабочий менеджер последовательностей.</translation>
    </message>
    <message>
        <source>Save Template</source>
        <translation>Сохранение шаблона</translation>
    </message>
    <message>
        <source>Failed to save template.</source>
        <translation>Не удалось сохранить шаблон.</translation>
    </message>
    <message>
        <source>Please select a template to delete.</source>
        <translation>Выберите шаблон для удаления.</translation>
    </message>
    <message>
        <source>Are you sure you want to delete the template '%1'?</source>
        <translation>Удалить шаблон «%1»?</translation>
    </message>
    <message>
        <source>Failed to delete template.</source>
        <translation>Не удалось удалить шаблон.</translation>
    </message>
    <message>
        <source>New Template</source>
        <translation>Новый шаблон</translation>
    </message>
    <message>
        <source>Template name:</source>
        <translation>Имя шаблона:</translation>
    </message>
    <message>
        <source>Template with this name already exists.</source>
        <translation>Шаблон с таким именем уже существует.</translation>
    </message>
    <message>
        <source>Failed to create template.</source>
        <translation>Не удалось создать шаблон.</translation>
    </message>
    <message>
        <source>Image not found</source>
        <translation>Изображение не найдено</translation>
    </message>
    <message>
        <source>Please select a template to export.</source>
        <translation>Выберите шаблон для экспорта.</translation>
    </message>
    <message>
        <source>ZIP files (*.zip);;All files (*)</source>
        <translation>ZIP-архивы (*.zip);;Все файлы (*)</translation>
    </message>
    <message>
        <source>Export Successful</source>
        <translation>Экспорт завершён</translation>
    </message>
    <message>
        <source>Template '%1' exported successfully to:
%2</source>
        <translation>Шаблон «%1» экспортирован в:
%2</translation>
    </message>
    <message>
        <source>Export Failed</source>
        <translation>Ошибка экспорта</translation>
    </message>
    <message>
        <source>Failed to export template. Check permissions and try again.</source>
        <translation>Не удалось экспортировать шаблон. Проверьте права доступа и повторите попытку.</translation>
    </message>
    <message>
        <source>Select Sequence to Import as Template</source>
        <translation>Выбор последовательности для импорта как шаблона</translation>
    </message>
    <message>
        <source>Sequence files (*.seq.json);;All files (*)</source>
        <translation>Файлы последовательностей (*.seq.json);;Все файлы (*)</translation>
    </message>
    <message>
        <source>Template Name</source>
        <translation>Имя шаблона</translation>
    </message>
    <message>
        <source>Enter template name:</source>
        <translation>Введите имя шаблона:</translation>
    </message>
    <message>
        <source>Template Description</source>
        <translation>Описание шаблона</translation>
    </message>
    <message>
        <source>Enter template description (optional):</source>
        <translation>Введите описание шаблона (необязательно):</translation>
    </message>
    <message>
        <source>Import Failed</source>
        <translation>Ошибка импорта</translation>
    </message>
    <message>
        <source>Template with name '%1' already exists. Please choose a different name.</source>
        <translation>Шаблон с именем «%1» уже существует. Выберите другое имя.</translation>
    </message>
    <message>
        <source>Import Successful</source>
        <translation>Импорт завершён</translation>
    </message>
    <message>
        <source>Sequence '%1' imported as template '%2' successfully!</source>
        <translation>Последовательность «%1» успешно импортирована как шаблон «%2»!</translation>
    </message>
    <message>
        <source>Failed to import sequence as template. Check permissions and try again.</source>
        <translation>Не удалось импортировать последовательность как шаблон. Проверьте права доступа и повторите попытку.</translation>
    </message>
</context>
<context>
    <name>SpeechRecognizer</name>
    <message>
        <source>Hearing: %1</source>
        <translation>Распознаётся: %1</translation>
    </message>
    <message>
        <source>Vosk: %1</source>
        <translation>Vosk: %1</translation>
    </message>
    <message>
        <source>Vosk: set libvosk.dll and the model folder in Settings &gt; Audio (models: alphacephei.com/vosk/models).</source>
        <translation>Vosk: укажите libvosk.dll и папку модели в «Настройки &gt; Аудио» (модели: alphacephei.com/vosk/models).</translation>
    </message>
    <message>
        <source>whisper.cpp: %1</source>
        <translation>whisper.cpp: %1</translation>
    </message>
    <message>
        <source>whisper.cpp: set the whisper-cli program and a ggml model in Settings &gt; Audio.</source>
        <translation>whisper.cpp: укажите программу whisper-cli и модель ggml в «Настройки &gt; Аудио».</translation>
    </message>
    <message>
        <source>Recognizing the recording...</source>
        <translation>Распознавание записи...</translation>
    </message>
    <message>
        <source>faster-whisper (Python, streaming)</source>
        <translation>faster-whisper (Python, потоковый)</translation>
    </message>
    <message>
        <source>Vosk (offline, libvosk.dll)</source>
        <translation>Vosk (офлайн, libvosk.dll)</translation>
    </message>
    <message>
        <source>whisper.cpp (command-line program)</source>
        <translation>whisper.cpp (консольная программа)</translation>
    </message>
    <message>
        <source>Omni ASR (Python, wav2vec2)</source>
        <translation>Omni ASR (Python, wav2vec2)</translation>
    </message>
</context>
<context>
    <name>TaskStatusController</name>
    <message>
        <source>Time: %1 | Context left: %2/%3 (%4%)</source>
        <translation>Время: %1 | Осталось контекста: %2/%3 (%4%)</translation>
    </message>
    <message>
        <source>%1M</source>
        <translation>%1млн</translation>
    </message>
    <message>
        <source>%1k</source>
        <translation>%1к</translation>
    </message>
</context>
<context>
    <name>TemplateSequenceGraphWidget</name>
    <message>
        <source>Template Information</source>
        <translation>Информация о шаблоне</translation>
    </message>
    <message>
        <source>Name:</source>
        <translation>Имя:</translation>
    </message>
    <message>
        <source>Description:</source>
        <translation>Описание:</translation>
    </message>
    <message>
        <source>Icon:</source>
        <translation>Значок:</translation>
    </message>
    <message>
        <source>No image selected</source>
        <translation>Изображение не выбрано</translation>
    </message>
    <message>
        <source>Select Image...</source>
        <translation>Выбрать изображение...</translation>
    </message>
    <message>
        <source>Available Sequences</source>
        <translation>Доступные последовательности</translation>
    </message>
    <message>
        <source>Template Sequences</source>
        <translation>Последовательности шаблона</translation>
    </message>
    <message>
        <source>→ Add to Template</source>
        <translation>→ Добавить в шаблон</translation>
    </message>
    <message>
        <source>Remove from Template ←</source>
        <translation>Убрать из шаблона ←</translation>
    </message>
    <message>
        <source>Set as Main</source>
        <translation>Сделать основной</translation>
    </message>
    <message>
        <source>Refresh Graph</source>
        <translation>Обновить граф</translation>
    </message>
    <message>
        <source>Template Info</source>
        <translation>О шаблоне</translation>
    </message>
    <message>
        <source>Sequences</source>
        <translation>Последовательности</translation>
    </message>
    <message>
        <source>Sequence Graph</source>
        <translation>Граф последовательностей</translation>
    </message>
    <message>
        <source>Image not found</source>
        <translation>Изображение не найдено</translation>
    </message>
    <message>
        <source>No Selection</source>
        <translation>Ничего не выбрано</translation>
    </message>
    <message>
        <source>Please select a sequence to add.</source>
        <translation>Выберите последовательность для добавления.</translation>
    </message>
    <message>
        <source>Create Directory</source>
        <translation>Создание папки</translation>
    </message>
    <message>
        <source>Could not create template directory: %1</source>
        <translation>Не удалось создать папку шаблона: %1</translation>
    </message>
    <message>
        <source>Add Sequence</source>
        <translation>Добавление последовательности</translation>
    </message>
    <message>
        <source>Sequence is already in template: %1</source>
        <translation>Последовательность уже есть в шаблоне: %1</translation>
    </message>
    <message>
        <source>Failed to copy sequence to template directory: %1</source>
        <translation>Не удалось скопировать последовательность в папку шаблона: %1</translation>
    </message>
    <message>
        <source>Please select a sequence to remove.</source>
        <translation>Выберите последовательность для удаления.</translation>
    </message>
    <message>
        <source>Please select a sequence to set as main.</source>
        <translation>Выберите последовательность, которую нужно сделать основной.</translation>
    </message>
    <message>
        <source>Select Image</source>
        <translation>Выбор изображения</translation>
    </message>
    <message>
        <source>Image Files (*.png *.jpg *.jpeg *.bmp *.svg *.gif)</source>
        <translation>Изображения (*.png *.jpg *.jpeg *.bmp *.svg *.gif)</translation>
    </message>
    <message>
        <source>Could not load image</source>
        <translation>Не удалось загрузить изображение</translation>
    </message>
    <message>
        <source>Template name is not set. Please set a template name first.</source>
        <translation>Имя шаблона не задано. Сначала укажите имя шаблона.</translation>
    </message>
    <message>
        <source>Could not create images directory: %1</source>
        <translation>Не удалось создать папку изображений: %1</translation>
    </message>
    <message>
        <source>Failed to copy image to template directory: %1</source>
        <translation>Не удалось скопировать изображение в папку шаблона: %1</translation>
    </message>
</context>
<context>
    <name>TextFileLines</name>
    <message>
        <source>Cannot open %1 for reading</source>
        <translation>Не удалось открыть %1 для чтения</translation>
    </message>
</context>
<context>
    <name>TtsController</name>
    <message>
        <source>Text to speech: On</source>
        <translation>Озвучка: Вкл</translation>
    </message>
    <message>
        <source>Text to speech: Off</source>
        <translation>Озвучка: Выкл</translation>
    </message>
</context>
<context>
    <name>TtsRunner</name>
    <message>
        <source>Text to speech: %1</source>
        <translation>Синтез речи: %1</translation>
    </message>
    <message>
        <source>Text to speech: scripts/run_tts_server.py was not found next to the program.</source>
        <translation>Синтез речи: scripts/run_tts_server.py не найден рядом с программой.</translation>
    </message>
</context>
<context>
    <name>UiBuilder</name>
    <message>
        <source>Browse...</source>
        <translation>Обзор...</translation>
    </message>
    <message>
        <source>Context Commands:</source>
        <translation>Команды контекста:</translation>
    </message>
    <message>
        <source>Commands</source>
        <translation>Команды</translation>
    </message>
    <message>
        <source>Context Graph</source>
        <translation>Граф контекста</translation>
    </message>
    <message>
        <source>Time</source>
        <translation>Время</translation>
    </message>
    <message>
        <source>Description</source>
        <translation>Описание</translation>
    </message>
    <message>
        <source>Settings</source>
        <translation>Настройки</translation>
    </message>
    <message>
        <source>Project</source>
        <translation>Проект</translation>
    </message>
    <message>
        <source>Model</source>
        <translation>Модель</translation>
    </message>
    <message>
        <source>Sequences</source>
        <translation>Последовательности</translation>
    </message>
    <message>
        <source>Graphs</source>
        <translation>Графики</translation>
    </message>
    <message>
        <source>Audio</source>
        <translation>Аудио</translation>
    </message>
    <message>
        <source>System</source>
        <translation>Система</translation>
    </message>
    <message>
        <source>Undo</source>
        <translation>Отменить</translation>
    </message>
    <message>
        <source>Run</source>
        <translation>Запустить</translation>
    </message>
    <message>
        <source>Project path</source>
        <translation>Путь к проекту</translation>
    </message>
    <message>
        <source>Actions</source>
        <translation>Действия</translation>
    </message>
    <message>
        <source>Rescan project</source>
        <translation>Обновить проект</translation>
    </message>
    <message>
        <source>Generate descriptions</source>
        <translation>Сгенерировать описания</translation>
    </message>
    <message>
        <source>Model settings</source>
        <translation>Настройки модели</translation>
    </message>
    <message>
        <source>Open LLM settings</source>
        <translation>Открыть настройки LLM</translation>
    </message>
    <message>
        <source>Edit</source>
        <translation>Редактировать</translation>
    </message>
    <message>
        <source>Create</source>
        <translation>Создать</translation>
    </message>
    <message>
        <source>Run template</source>
        <translation>Запустить шаблон</translation>
    </message>
    <message>
        <source>Edit templates</source>
        <translation>Редактировать шаблоны</translation>
    </message>
    <message>
        <source>Export as template</source>
        <translation>Экспорт как шаблон</translation>
    </message>
    <message>
        <source>Visualization</source>
        <translation>Визуализация</translation>
    </message>
    <message>
        <source>Context graph</source>
        <translation>Граф контекста</translation>
    </message>
    <message>
        <source>Sequence graph</source>
        <translation>Граф последовательностей</translation>
    </message>
    <message>
        <source>LightRAG</source>
        <translation>LightRAG</translation>
    </message>
    <message>
        <source>Documents: search and questions</source>
        <translation>Документы: поиск и вопросы</translation>
    </message>
    <message>
        <source>Index the project's documents and ask questions about them. The agent can then search them with SEARCH_DOCS.</source>
        <translation>Проиндексировать документы проекта и задавать по ним вопросы. После этого агент сможет искать по ним командой SEARCH_DOCS.</translation>
    </message>
    <message>
        <source>Voice input</source>
        <translation>Голосовой ввод</translation>
    </message>
    <message>
        <source>Engine</source>
        <translation>Движок</translation>
    </message>
    <message>
        <source>Vosk library</source>
        <translation>Библиотека Vosk</translation>
    </message>
    <message>
        <source>libvosk.dll</source>
        <translation>libvosk.dll</translation>
    </message>
    <message>
        <source>Vosk model</source>
        <translation>Модель Vosk</translation>
    </message>
    <message>
        <source>folder, e.g. vosk-model-small-ru-0.22</source>
        <translation>папка, например vosk-model-small-ru-0.22</translation>
    </message>
    <message>
        <source>whisper.cpp program</source>
        <translation>Программа whisper.cpp</translation>
    </message>
    <message>
        <source>whisper-cli.exe</source>
        <translation>whisper-cli.exe</translation>
    </message>
    <message>
        <source>whisper.cpp model</source>
        <translation>Модель whisper.cpp</translation>
    </message>
    <message>
        <source>ggml-small.bin</source>
        <translation>ggml-small.bin</translation>
    </message>
    <message>
        <source>Python</source>
        <translation>Python</translation>
    </message>
    <message>
        <source>python from PATH</source>
        <translation>python из PATH</translation>
    </message>
    <message>
        <source>detect automatically, or a code such as ru, en</source>
        <translation>определять автоматически, или код языка, например ru, en</translation>
    </message>
    <message>
        <source>Language (whisper.cpp)</source>
        <translation>Язык (whisper.cpp)</translation>
    </message>
    <message>
        <source>Text to speech</source>
        <translation>Озвучка</translation>
    </message>
    <message>
        <source>Security</source>
        <translation>Безопасность</translation>
    </message>
    <message>
        <source>Run read-only commands, ask for the rest</source>
        <translation>Выполнять команды только для чтения, об остальных спрашивать</translation>
    </message>
    <message>
        <source>Ask before every command</source>
        <translation>Спрашивать перед каждой командой</translation>
    </message>
    <message>
        <source>Run everything without asking (unsafe)</source>
        <translation>Выполнять всё без вопросов (небезопасно)</translation>
    </message>
    <message>
        <source>Read-only commands open and search files. Commands that change files, run programs or delete anything are confirmed in a dialog that shows exactly what they will do.</source>
        <translation>Команды только для чтения открывают файлы и ищут по ним. Команды, которые изменяют файлы, запускают программы или что-либо удаляют, подтверждаются в окне, где точно показано, что они сделают.</translation>
    </message>
    <message>
        <source>Agent mode</source>
        <translation>Режим агента</translation>
    </message>
    <message>
        <source>Plan each step with a separate model call (slower, no streaming)</source>
        <translation>Планировать каждый шаг отдельным запросом к модели (медленнее, без потокового вывода)</translation>
    </message>
    <message>
        <source>Each step first asks the model for a plan and then for the commands. It uses the model selected in the chat.</source>
        <translation>На каждом шаге модель сначала составляет план, а затем выдаёт команды. Используется модель, выбранная в чате.</translation>
    </message>
    <message>
        <source>Interface</source>
        <translation>Интерфейс</translation>
    </message>
    <message>
        <source>As in the system</source>
        <translation>Как в системе</translation>
    </message>
    <message>
        <source>Restart DevAssistant to switch the language.</source>
        <translation>Перезапустите DevAssistant, чтобы сменить язык.</translation>
    </message>
    <message>
        <source>Language</source>
        <translation>Язык</translation>
    </message>
    <message>
        <source>Show chat</source>
        <translation>Показать чат</translation>
    </message>
    <message>
        <source>Show log</source>
        <translation>Показать лог</translation>
    </message>
</context>
<context>
    <name>WorkflowLibrary</name>
    <message>
        <source>Template not found: %1</source>
        <translation>Шаблон не найден: %1</translation>
    </message>
    <message>
        <source>The template "%1" has no sequence that can be loaded.</source>
        <translation>В шаблоне «%1» нет последовательности, которую можно загрузить.</translation>
    </message>
</context>
</TS>