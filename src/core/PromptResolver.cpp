// PromptResolver.cpp
#include "PromptResolver.h"
#include "MainWindow.h"
#include "PromptManager.h"
#include "../core/ContextChainResolver.h"
#include "../core/ProjectStructureResolver.h"
#include "../command/CommandDocs.h"
#include <QMap>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QtGlobal>
#include <QJsonDocument>
#include "utils/Logging.h"

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

static QString truncateLinesForPrompt(const QString& text, int maxLines, int maxChars)
{
    QStringList lines = text.split('\n', QString::KeepEmptyParts);
    if (maxLines > 0 && lines.size() > maxLines) {
        const int head = qMax(1, maxLines - 50);
        const int tail = qMin(50, maxLines / 4);
        const QStringList kept = lines.mid(0, head) + QStringList{QStringLiteral("...")} + lines.mid(lines.size() - tail);
        lines = kept;
    }
    QString out = lines.join("\n");
    if (maxChars > 0 && out.size() > maxChars) {
        out = out.left(maxChars);
        out += QStringLiteral("\n...");
    }
    return out;
}

static QString formatUtcOffset(int offsetSeconds)
{
    const int absSeconds = qAbs(offsetSeconds);
    const int hours = absSeconds / 3600;
    const int minutes = (absSeconds % 3600) / 60;
    const QChar sign = (offsetSeconds >= 0) ? QChar('+') : QChar('-');
    return QStringLiteral("UTC%1%2:%3")
        .arg(sign)
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'));
}

PromptResolver::ResolvedPrompt PromptResolver::resolve(const QString &tmpl,
                                                       const PromptStep &step,
                                                       MainWindow *ctx)
{
    if (!ctx) {
        // Nothing to fill the placeholders from: return the template as it is.
        ResolvedPrompt plain;
        plain.userPrompt = tmpl;
        return plain;
    }
    QString result = tmpl;
    QString userQuery = ctx->lastUserMessage();
    const QDateTime now = QDateTime::currentDateTime();
    const QString currentDate = now.date().toString(QStringLiteral("yyyy-MM-dd"));
    const QString currentDateTime = QStringLiteral("%1 %2 (%3)")
        .arg(currentDate,
             now.time().toString(QStringLiteral("HH:mm:ss")),
             formatUtcOffset(now.offsetFromUtc()));
    const QString runtimeDateContext = QStringLiteral(
        "Runtime date context:\n"
        "- CURRENT_DATE: %1\n"
        "- CURRENT_DATETIME: %2\n"
        "- Interpret relative dates (today/now) using CURRENT_DATE.\n\n")
        .arg(currentDate, currentDateTime);
    qCDebug(logPrompt) << "[PromptResolver] USER_QUERY =" << userQuery;

    // 1) {{PROJECT_ROOT}}
    result.replace("{{PROJECT_ROOT}}", ctx->projectRootPath());

    // 2) {{FILE_LIST}}
    const QString fileListAbs = absoluteFileList(ctx->promptManager()->fileListText(),
                                                 ctx->projectRootPath());
    result.replace("{{FILE_LIST}}", truncateLinesForPrompt(fileListAbs, 600, 18000));

    // 3) {{HISTORY}} – берём последние N сообщений из MainWindow
    QString hist;
    const int maxHist = 12;
    const auto &history = ctx->history();
    int start = qMax(0, history.size() - maxHist);
    for (int i = start; i < history.size(); ++i) {
        QString msg = history[i].second;
        if (msg.size() > 800)
            msg = msg.left(800) + QStringLiteral("...");
        hist += history[i].first + ": " + msg + "\n";
    }
    result.replace("{{HISTORY}}", hist.trimmed());

    // 3.1) {{LAST_USER_MESSAGES}} – берём последние N пользовательских сообщений из отдельного хранилища
    QString userMessages = ctx->getLastUserMessages();
    qCDebug(logPrompt) << "[PromptResolver] LAST_USER_MESSAGES content:" << userMessages;
    qCDebug(logPrompt) << "[PromptResolver] LAST_USER_MESSAGES count:" << userMessages.split('\n').size();
    result.replace("{{LAST_USER_MESSAGES}}", userMessages);

    // 4) {{OPEN_FILES}} – содержимое файлов, открытых через OPEN_FILE
    result.replace("{{OPEN_FILES}}", ctx->openedFilesContent());

    // 5) {{SUPPORTED_COMMANDS}} – список только разрешённых для шага
    QString allowedList;
    const auto all = CommandRegistry::all(); // шаблоны всех команд
    QSet<QString> allowedSet = QSet<QString>::fromList(step.allowedCommands());

    for (Command *c : all) {
        if (!allowedSet.contains(c->name())) {
            delete c;
            continue;
        }

        const CommandDoc doc = CommandDocs::doc(c->name());
        // Small-model friendly: keep command docs compact to reduce token pressure.
        allowedList += CommandDocs::renderForLLM(doc, true) + "\n\n";
        delete c;
        // Формат как в PromptManager::supportedCommandsText()

        continue;
        QJsonObject example = c->toJson();
        QString json = QString::fromStdString(QJsonDocument(example).toJson(QJsonDocument::Compact).toStdString());
        allowedList += "- " + c->name() + ": " + c->description() + "\n  Формат: " + json + "\n\n";
        delete c;
    }
    result.replace("{{SUPPORTED_COMMANDS}}", allowedList.trimmed());

    // 6) {{USER_QUERY}} – текущий запрос пользователя
    result.replace("{{USER_QUERY}}", userQuery);
    result.replace("{{CURRENT_DATE}}", currentDate);
    result.replace("{{TODAY}}", currentDate);
    result.replace("{{CURRENT_DATETIME}}", currentDateTime);

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
    
    // 7) {{OPEN_SEQUENCE_INFO}} – список доступных последовательностей в шаблоне
    if (result.contains("{{OPEN_SEQUENCE_INFO}}")) {
        QString sequenceInfoList = generateSequenceInfoList(ctx);
        result.replace("{{OPEN_SEQUENCE_INFO}}", sequenceInfoList);
    }
    
    // 8) {{PROJECT_STRUCTURE}} – древовидное представление структуры проекта
    result.replace("{{PROJECT_STRUCTURE}}", ProjectStructureResolver::resolveProjectStructure(ctx));
    

    
    // Удаляем дублирующий вызов из строки 92
    // result.replace("{{OPEN_FILES}}", ctx->openedFilesContent());

    // 8) Любой неизвестный {{…}} – просто удаляем, чтобы LLM не падал
    // Но делаем это только ПОСЛЕ замены всех известных шаблонов
    result.remove(QRegularExpression(R"(\{\{[^\}]+\}\})"));
    result.prepend(runtimeDateContext);

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

    vec.append(ResolverInfo{"PROJECT_ROOT",    "Корневая папка проекта"});
    vec.append(ResolverInfo{"FILE_LIST",       "Список файлов проекта"});
    vec.append(ResolverInfo{"HISTORY",         "Последние 20 сообщений из чата"});
    vec.append(ResolverInfo{"LAST_USER_MESSAGES", "Последние 100 пользовательских сообщений из чата"});
    vec.append(ResolverInfo{"OPEN_FILES",      "Содержимое файлов, открытых командой OPEN_FILE"});
    vec.append(ResolverInfo{"SUPPORTED_COMMANDS", "Список доступных команд в данном шаге"});
    vec.append(ResolverInfo{"USER_QUERY",      "Текущий запрос пользователя"});
    vec.append(ResolverInfo{"CURRENT_DATE",    "Текущая дата (YYYY-MM-DD)"});
    vec.append(ResolverInfo{"TODAY",           "Синоним текущей даты (YYYY-MM-DD)"});
    vec.append(ResolverInfo{"CURRENT_DATETIME","Текущие дата/время с UTC-смещением"});
    vec.append(ResolverInfo{"LAST_ASSISTANTS_RESPONSE",      "Последний ответ ассистент"});
    vec.append(ResolverInfo{"OPEN_FILES",      "Открытые файлы"});
    vec.append(ResolverInfo{"FILE_DESCRIPTIONS", "Добавить описание файлов проекта"});
    vec.append(ResolverInfo{"PREVIOUS_STEPS", "Что было уже сделано на предыдущих этапах"});
    vec.append(ResolverInfo{"TOKEN_COUNT", "Число токенов текущего контекста"});
    vec.append(ResolverInfo{"SEARCH_RESULTS", "Краткие выдержки из SEARCH_WEB / SEARCH_PROJECT"});
    vec.append(ResolverInfo{"OPEN_SEQUENCE_INFO", "Список доступных последовательностей в шаблоне в формате: номер. описание; sequenceName = \"имя_файла_без_.seq.json\"; summarize = true;"});
    vec.append(ResolverInfo{"CONTEXT_CHAIN", "Цепочка контекста из выполненных команд с визуальным статусом"});
    vec.append(ResolverInfo{"PROJECT_STRUCTURE", "Древовидное представление структуры папок проекта"});

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

QString PromptResolver::generateSequenceInfoList(MainWindow* ctx)
{
    if (!ctx) {
        return QString("Нет доступных последовательностей: контекст отсутствует");
    }
    
    // Get the current template directory based on the current sequence path
    QString templateDir = ctx->currentSequencePath();
    QString currentSequenceName;
    
    if (!templateDir.isEmpty()) {
        QFileInfo seqPathInfo(templateDir);
        currentSequenceName = seqPathInfo.baseName();  // Get the base name (without extension) of the current sequence file
        templateDir = seqPathInfo.absolutePath();      // Get the directory containing the sequence file
        
    } else {
        // If no current sequence path, we might be in a general context
        templateDir.clear();
    }
    
    QStringList sequenceInfoList;
    int index = 1;
    
    // Look for the main template file to read sequence targets (directed links)
    if (!templateDir.isEmpty() && QDir(templateDir).exists()) {
        // Look for template.templ.json in the same directory
        QString mainTemplatePath = QDir(templateDir).filePath("template.templ.json");
        
        if (QFile::exists(mainTemplatePath)) {
            // Load the main template to get sequence targets
            QFile templateFile(mainTemplatePath);
            if (templateFile.open(QIODevice::ReadOnly)) {
                QByteArray templateData = templateFile.readAll();
                QJsonDocument templateDoc = QJsonDocument::fromJson(templateData);
                
                if (templateDoc.isObject()) {
                    QJsonObject templateObj = templateDoc.object();
                    
                    // Look for sequence targets object in the template
                    if (templateObj.contains("sequenceTargets")) {
                        QJsonObject sequenceTargets = templateObj.value("sequenceTargets").toObject();
                        
                        // Get the targets for the current sequence specifically
                        QJsonArray currentTargets;
                        if (!currentSequenceName.isEmpty() && sequenceTargets.contains(currentSequenceName)) {
                            currentTargets = sequenceTargets.value(currentSequenceName).toArray();
                            
                            // Process each target sequence
                            for (const QJsonValue& targetValue : currentTargets) {
                                QString seqBaseName = targetValue.toString();
                                
                                // Find the sequence file and get its details
                                QString seqPath = QDir(templateDir).filePath(seqBaseName + ".seq.json");
                                QString seqName = seqBaseName;  // Default to filename
                                QString seqDescription = "";
                                
                                QFile seqFile(seqPath);
                                if (seqFile.open(QIODevice::ReadOnly)) {
                                    QByteArray data = seqFile.readAll();
                                    QJsonDocument doc = QJsonDocument::fromJson(data);
                                    if (doc.isObject()) {
                                        PromptSequence seq = PromptSequence::fromJson(doc.object());
                                        seqName = seq.name();
                                        seqDescription = seq.description();
                                    }
                                    seqFile.close();
                                }
                                
                                // Use the sequence name as description if no specific description provided
                                if (seqDescription.isEmpty()) {
                                    seqDescription = seqName;
                                }
                                
                                QString sequenceInfo = QString("%1. %2; sequenceName = \"%3\"; summarize = true;")
                                    .arg(index)
                                    .arg(seqDescription.isEmpty() ? seqBaseName : seqDescription)
                                    .arg(seqBaseName);
                                    
                                sequenceInfoList << sequenceInfo;
                                index++;
                            }
                        }
                    }
                }
            }
        }
        
        // If no directed links were found for current sequence, fallback to all sequences in the directory
        if (sequenceInfoList.isEmpty()) {
            QDir dir(templateDir);
            QStringList filters = {"*.seq.json"};
            QFileInfoList fileInfoList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);
            
            for (const QFileInfo& fileInfo : fileInfoList) {
                QString seqPath = fileInfo.absoluteFilePath();
                
                // Load the sequence from file to get its name and description
                QFile seqFile(seqPath);
                if (!seqFile.open(QIODevice::ReadOnly)) {
                    continue;
                }
                
                QByteArray data = seqFile.readAll();
                QJsonDocument doc = QJsonDocument::fromJson(data);
                if (!doc.isObject()) {
                    continue;  // Skip invalid JSON files
                }
                
                PromptSequence seq = PromptSequence::fromJson(doc.object());
                
                QString seqName = seq.name();
                QString seqDescription = seq.description();
                if (seqDescription.isEmpty()) {
                    seqDescription = seqName;  // Use name as description if no description is available
                }
                
                QString seqBaseName = fileInfo.baseName();  // Get filename without extension
                
                QString sequenceInfo = QString("%1. %2; sequenceName = \"%3\"; summarize = false;")
                    .arg(index)
                    .arg(seqDescription)
                    .arg(seqBaseName);
                    
                sequenceInfoList << sequenceInfo;
                index++;
            }
        }
    } else {
        // Fallback: use sequence manager if no template directory
        PromptSequenceManager& seqMgr = ctx->sequenceManager();
        const QMap<QString, PromptSequence>& allSequences = seqMgr.sequences();
        
        for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
            const PromptSequence& seq = it.value();
            QString seqDescription = seq.description();
            if (seqDescription.isEmpty()) {
                seqDescription = seq.name();  // Use name as description if no description is available
            }
            
            QFileInfo seqFileInfo(it.key());  // it.key() is the file path
            QString seqBaseName = seqFileInfo.baseName();  // Get filename without extension
            
            QString sequenceInfo = QString("%1. %2; sequenceName = \"%3\"; summarize = false;")
                .arg(index)
                .arg(seqDescription)
                .arg(seqBaseName);
                
            sequenceInfoList << sequenceInfo;
            index++;
        }
    }
    
    if (sequenceInfoList.isEmpty()) {
        return QString("Нет доступных последовательностей");
    }
    
    return sequenceInfoList.join("\n");
}
