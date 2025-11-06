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
    
    // 7) {{OPEN_SEQUENCE_INFO}} – список доступных последовательностей в шаблоне
    if (result.contains("{{OPEN_SEQUENCE_INFO}}")) {
        QString sequenceInfoList = generateSequenceInfoList(ctx);
        result.replace("{{OPEN_SEQUENCE_INFO}}", sequenceInfoList);
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
    vec.append({"OPEN_SEQUENCE_INFO", "Список доступных последовательностей в шаблоне в формате: номер. описание; sequenceName = \"имя_файла_без_.seq.json\"; summarize = true;"});
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
