// PromptResolver.cpp
#include "PromptResolver.h"
#include "MainWindow.h"
#include "PromptManager.h"
#include <QMap>
#include <QRegularExpression>
#include <QDebug>

QString PromptResolver::resolve(const QString &tmpl,
                               const PromptStep &step,
                               MainWindow *ctx)
{
    QString result = tmpl;

    // 1) {{PROJECT_ROOT}}
    result.replace("{{PROJECT_ROOT}}", ctx->projectRootPath());

    // 2) {{FILE_LIST}}
    result.replace("{{FILE_LIST}}", ctx->promptManager()->fileListText());

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
    result.replace("{{USER_QUERY}}", ctx->lastUserMessage());

    // 7) {{OPEN_FILES}} – текущий запрос пользователя
    result.replace("{{OPEN_FILES}}", ctx->openedFilesContent());

    // 8) Любой неизвестный {{…}} – просто удаляем, чтобы LLM не падал
    result.remove(QRegularExpression(R"(\{\{[^\}]+\}\})"));



    return result;
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
    vec.append({"OPEN_FILES",      "Открытые файлы"});

    return vec;
}
