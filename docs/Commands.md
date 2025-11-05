# Команды DevAssistantQt

## Файловые команды

### CREATE_FILE
Создание нового файла
- **Параметры**: `path`, `content`

### REPLACE_IN_FILE  
Замена содержимого файла
- **Параметры**: `path`, `content`

### DELETE_FILE
Удаление файла
- **Параметры**: `path`

### RENAME_FILE
Переименование файла
- **Параметры**: `oldPath`, `newPath`

### EDIT_IN_RANGE
Редактирование части файла в заданных строках
- **Параметры**: `path`, `startLine`, `endLine`, `content`, `replacementLines`

## Команды открытия файлов

### OPEN_FILE (или OPEN_FILE_CONTEXT)
Открытие файла для просмотра
- **Параметры**: `path`

### OPEN_FILE_REGION
Открытие конкретной области файла
- **Параметры**: `path`, `region`, `startLine`, `endLine`

## Выполнение команд

### RUN_PYTHON
Выполнение Python-скрипта
- **Параметры**: `scriptPath`, `code`

### RUN_SHELL
Выполнение shell-команды
- **Параметры**: `command`

## Поисковые команды

### SEARCH_WEB
Поиск в интернете
- **Параметры**: `query`, `maxResults`

### SEARCH_PROJECT
Поиск по проекту (с опциональной чувствительностью к регистру)
- **Параметры**: `query`, `maxMatches`, `caseSensitive`, `include`

### SEARCH_PROJECT_CASE_INSENSITIVE
Поиск по проекту без учета регистра (новая команда)
- **Параметры**: `query`, `maxMatches`, `include`

## Взаимодействие с пользователем

### ASK_USER
Задать вопрос пользователю
- **Параметры**: `question`

## Работа с описаниями

### WRITE_DESCRIPTION
Написать описание для файла
- **Параметры**: `sourceFilePath`, `description`

## Работа с последовательностями

### CALL_SEQUENCE
Запустить последовательность команд
- **Параметры**: `sequenceName`, `summarize`

### OPEN_SEQUENCE
Открыть последовательность
- **Параметры**: `sequenceName`

## Вспомогательные команды

### SHOW_RESULT
Показать результат
- **Параметры**: `content`, `title`