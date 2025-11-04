// PromptResolver.cpp
#include "PromptResolver.h"
#include "MainWindow.h"
#include "PromptManager.h"
#include "../core/ContextChainResolver.h"
#include <QMap>
#include <QRegularExpression>
#include <QDebug>
#include <QFileInfo>
#include <QDir>

// Static members initialization
QString PromptResolver::m_nextSequenceName;
QString PromptResolver::m_nextSequenceDescription;

static QString absoluteFileList(const QString& list, const QString& root)
{
    QStringList abs;
    QDir rootDir(root);
    const QStringList lines = list.split('\n', QString::KeepEmptyParts);
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;
        QFileInfo info(trimmed);
        if (!info.isAbsolute()) {
            trimmed = rootDir.absoluteFilePath(trimmed);
        }
        abs << QDir::toNativeSeparators(trimmed);
    }
    return abs.join('\n');
}

PromptResolver::ResolvedPrompt PromptResolver::resolve(const QString &tmpl,
                                                       const PromptStep &step,
                                                       MainWindow *ctx)
{
    QString result = tmpl;
    QString userQuery = ctx ? ctx->lastUserMessage() : QString();
    qDebug() << "[PromptResolver] USER_QUERY =" << userQuery;

    // 1) {{PROJECT_ROOT}}
    result.replace("{{PROJECT_ROOT}}", ctx->projectRootPath());

    // 2) {{FILE_LIST}}
    result.replace("{{FILE_LIST}}", absoluteFileList(ctx->promptManager()->fileListText(),
                                                     ctx->projectRootPath()));

    // 3) {{HISTORY}} – берём последние N сообщений из MainWindow
    QString hist;
    const int maxHist = 20;
    const auto &history = ctx->history();
    int start = qMax(0, history.size() - maxHist);
    for (int i = start; i < history.size(); ++i) {
        hist += history[i].first + ": " + history[i].second + "\n";
    }
    result.replace("{{HISTORY}}", hist.trimmed());

    // 4) {{OPEN_FILES}} – содержимое файлов, открытых через OPEN_FILE
    result.replace("{{OPEN_FILES}}", ctx->openedFilesContent());

    // 5) {{SUPPORTED_COMMANDS}} – список только разрешённых для шага
    QString allowedList;
    const auto all = CommandRegistry::all(); // шаблоны всех команд
    QSet<QString> allowedSet = QSet<QString>::fromList(step.allowedCommands());

    for (Command *c : all) {
        if (!allowedSet.contains(c->name()))
            continue;
        // Формат как в PromptManager::supportedCommandsText()

        QJsonObject example = c->toJson();
        QString json = QString::fromStdString(QJsonDocument(example).toJson(QJsonDocument::Compact).toStdString());
        allowedList += "- " + c->name() + "\n  Формат: " + json + "\n\n";
        delete c;
    }
    result.replace("{{SUPPORTED_COMMANDS}}", allowedList.trimmed());

    // 6) {{USER_QUERY}} – текущий запрос пользователя
    result.replace("{{USER_QUERY}}", userQuery);

    // Последний ответ ассистента
    result.replace("{{LAST_ASSISTANTS_RESPONSE}}", ctx->lastAssistantMessage());

    // 7.1) {{SEARCH_RESULTS}} – результаты поисковых команд
    result.replace("{{SEARCH_RESULTS}}", ctx->searchResultsText());

    // 7.2) {{CONTEXT_CHAIN}} – context chain from executed commands
    result.replace("{{CONTEXT_CHAIN}}", ContextChainResolver::resolveContextChain(ctx));

    
    // 6.1) {{FILE_DESCRIPTIONS}} — описания из папки DevelopersAssistant
    result.replace("{{FILE_DESCRIPTIONS}}", ctx->fileDescriptionsText());

    // 6.2) {{PREVIOUS_STEPS}} — краткая выжимка по уже выполненным шагам
    result.replace("{{PREVIOUS_STEPS}}", ctx->previousStepsText());

    // 6.3) {{TOKEN_COUNT}} — приблизительная оценка токенов для этого шаблона
    {
        // Простейшая эвристика: ~4 символа на токен
        int approxTokens = (result.size() / 4) + 1;
        result.replace("{{TOKEN_COUNT}}", QString::number(approxTokens));
    }
    
    // 7) {{OPEN_SEQUENCE:sequenceName:description}} – указывает какую последовательность открыть следующей
    QRegularExpression openSeqRegex(R"(\{\{OPEN_SEQUENCE:([^,}]+),?([^}]*)\}\})");
    QRegularExpressionMatchIterator iter = openSeqRegex.globalMatch(result);
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString sequenceName = match.captured(1).trimmed();
        QString description = match.captured(2).trimmed();
        
        // Set the next sequence for main window to process
        setNextSequence(sequenceName, description);
        
        // Replace placeholder with empty string to remove it from the prompt
        result = result.mid(0, match.capturedStart()) + result.mid(match.capturedEnd());
    }
    
    // Удаляем дублирующий вызов из строки 92
    // result.replace("{{OPEN_FILES}}", ctx->openedFilesContent());

    // 8) Любой неизвестный {{…}} – просто удаляем, чтобы LLM не падал
    // Но делаем это только ПОСЛЕ замены всех известных шаблонов
    result.remove(QRegularExpression(R"(\{\{[^\}]+\}\})"));

    // Добавляем четкие инструкции о формате JSON-ответа
    // Это поможет нейросети лучше понять, что нужно возвращать JSON команды
    /*result += "\n\nВАЖНО: ";
    result += "ВАЖНО: Отвечай только JSON-массивом команд в правильном формате. ";
    result += "Не добавляй пояснительный текст вне JSON. ";
    result += "Используй только команды из списка 'Доступные команды'. ";
    result += "Каждая команда должна быть валидным JSON-объектом с полями 'command' и 'params'.\n";
    result += "Формат: [{\"command\": \"НАЗВАНИЕ_КОМАНДЫ\", \"params\": {...}}, ...]";
*/
    ResolvedPrompt out;
    out.systemPrompt = result;
    out.userPrompt = userQuery;
    return out;
}

QVector<PromptResolver::ResolverInfo> PromptResolver::availableResolvers()
{
    QVector<ResolverInfo> vec;

    vec.append({"PROJECT_ROOT",    "Корневая папка проекта"});
    vec.append({"FILE_LIST",       "Список файлов проекта"});
    vec.append({"HISTORY",         "Последние 20 сообщений из чата"});
    vec.append({"OPEN_FILES",      "Содержимое файлов, открытых командой OPEN_FILE"});
    vec.append({"SUPPORTED_COMMANDS", "Список доступных команд в данном шаге"});
    vec.append({"USER_QUERY",      "Текущий запрос пользователя"});
    vec.append({"LAST_ASSISTANTS_RESPONSE",      "Последний ответ ассистент"});
    vec.append({"OPEN_FILES",      "Открытые файлы"});
    vec.append({"FILE_DESCRIPTIONS", "Добавить описание файлов проекта"});
    vec.append({"PREVIOUS_STEPS", "Что было уже сделано на предыдущих этапах"});
    vec.append({"TOKEN_COUNT", "Число токенов текущего контекста"});
    vec.append({"SEARCH_RESULTS", "Краткие выдержки из SEARCH_WEB / SEARCH_PROJECT"});
    vec.append({"OPEN_SEQUENCE", "Указать, какую последовательность запустить следующей. Формат: {{OPEN_SEQUENCE:имя_последовательности,описание}"});
    vec.append({"CONTEXT_CHAIN", "Цепочка контекста из выполненных команд с визуальным статусом"});

    return vec;
}

void PromptResolver::setNextSequence(const QString& sequenceName, const QString& description)
{
    m_nextSequenceName = sequenceName;
    m_nextSequenceDescription = description;
}

QString PromptResolver::getNextSequence()
{
    if (m_nextSequenceName.isEmpty()) {
        return QString();
    }
    return m_nextSequenceName;
}

void PromptResolver::clearNextSequence()
{
    m_nextSequenceName.clear();
    m_nextSequenceDescription.clear();
}
