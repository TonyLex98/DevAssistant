#include "PromptManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>

PromptManager::PromptManager(){}

QString PromptManager::basePromptPath() const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty()) dir = QDir::homePath()+"/DevAssistantQt";
    QDir().mkpath(dir);
    return dir + "/base_prompt.txt";
}

QString PromptManager::loadBasePrompt() const {
    QFile f(basePromptPath());
    if (!f.exists()) {
        QString def =
            "Представь, что ты AI-ассистент, который может выполнять ЗАРАНЕЕ ОПРЕДЕЛЕННЫЕ КОМАНДЫ DSL.\n"
            "Всегда отвечай ТОЛЬКО этими командами. Никаких объяснений.\n"
            "Если действие невозможно — верни пустой ответ.\n";
        QFile w(basePromptPath()); if (w.open(QIODevice::WriteOnly|QIODevice::Truncate)) { w.write(def.toUtf8()); w.close(); }
        return def;
    }
    if (!f.open(QIODevice::ReadOnly)) return QString();
    QString s = QString::fromUtf8(f.readAll()); f.close();
    return s;
}

void PromptManager::saveBasePrompt(const QString& text) const {
    QFile f(basePromptPath()); if (f.open(QIODevice::WriteOnly|QIODevice::Truncate)) { f.write(text.toUtf8()); f.close(); }
}

QString PromptManager::buildPrompt(const QString& userText,
                                   const QString& projectContext,
                                   const QString& capabilities,
                                   const QList<QPair<QString,QString>>& history) const
{
    QString p;
    p += loadBasePrompt();
    p += "\n\n";
    p += capabilities;
    p += "\n\nPROJECT CONTEXT:\n";
    p += projectContext;
    p += "\n\nPREVIOUS MESSAGES (latest "+QString::number(m_historySize)+"):\n";
    int start = history.size() > m_historySize ? history.size()-m_historySize : 0;
    for (int i=start;i<history.size();++i) {
        const auto& h = history[i];
        p += h.first + ": " + h.second + "\n";
    }
    p += "\nUSER NOW: " + userText + "\n";
    p += "Ответь ТОЛЬКО командами DSL без пояснений.\n";
    return p;
}
