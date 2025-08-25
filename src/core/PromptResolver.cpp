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
        QString json = QString::fromUtf8(QJsonDocument(example).toJson(QJsonDocument::Compact));
        allowedList += "- " + c->name() + "\n  Формат: " + json + "\n\n";
        delete c;
    }
    result.replace("{{SUPPORTED_COMMANDS}}", allowedList.trimmed());

    // 6) Любой неизвестный {{…}} – просто удаляем, чтобы LLM не падал
    result.remove(QRegularExpression(R"(\{\{[^\}]+\}\})"));

    return result;
}
