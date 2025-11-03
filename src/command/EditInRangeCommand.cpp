#include "EditInRangeCommand.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDateTime>
#include <algorithm>

static QStringList splitPreserveEmpty(const QString& text)
{
    return text.split('\n', QString::KeepEmptyParts);
}

EditInRangeCommand::EditInRangeCommand(const QString& path,
                                       int startLine, int endLine,
                                       const QByteArray& content,
                                       const QJsonArray& replacementLines,
                                       const QString& descr)
    : m_path(path),
      m_startLine(startLine),
      m_endLine(endLine)
{
    m_params["path"]      = path;
    m_params["startLine"] = startLine;
    m_params["endLine"]   = endLine;

    m_description = descr.isEmpty()
        ? QString("Edit lines %1-%2 in \"%3\" (line-by-line patch).")
              .arg(startLine).arg(endLine).arg(path)
        : descr;

    if (!replacementLines.isEmpty()) {
        QJsonArray normalized;
        for (int i = 0; i < replacementLines.size(); ++i) {
            const QJsonObject obj = replacementLines.at(i).toObject();
            const int lineNo = obj.value("line").toInt(startLine + i);
            const QString textLine = obj.value("text").toString();
            m_newLines << textLine;
            normalized.append(QJsonObject{
                { "line", lineNo },
                { "text", textLine }
            });
        }
        m_params["replacementLines"] = normalized;
        m_params["content"] = m_newLines.join('\n');
    } else {
        const QString asText = QString::fromUtf8(content);
        m_params["content"] = asText;
        m_newLines = splitPreserveEmpty(asText);

        QJsonArray normalized;
        for (int i = 0; i < m_newLines.size(); ++i) {
            normalized.append(QJsonObject{
                { "line", startLine + i },
                { "text", m_newLines.at(i) }
            });
        }
        m_params["replacementLines"] = normalized;
    }
}

bool EditInRangeCommand::apply(DevAssManager* devAss, QString* err)
{
    QList<EditInRangeCommand*> single{ this };
    return applyBatch(single, devAss, err);
}

bool EditInRangeCommand::applyBatch(const QList<EditInRangeCommand*>& commands,
                                    DevAssManager* devAss,
                                    QString* err)
{
    if (commands.isEmpty())
        return true;

    QString path = commands.first()->path();
    // Нормализуем путь: если путь относительный, делаем его относительно корневой директории проекта
    if (devAss && !QFileInfo(path).isAbsolute()) {
        path = QDir(devAss->projectRoot().absolutePath()).filePath(path);
        // Обновляем путь в параметрах команды для отображения в результатах
        const_cast<EditInRangeCommand*>(commands.first())->m_path = path;
        const_cast<EditInRangeCommand*>(commands.first())->m_params["path"] = path;
    }

    for (const EditInRangeCommand* cmd : commands) {
        QString cmdPath = cmd->path();
        // Нормализуем путь команды для сравнения
        if (devAss && !QFileInfo(cmdPath).isAbsolute()) {
            cmdPath = QDir(devAss->projectRoot().absolutePath()).filePath(cmdPath);
            // Обновляем путь в параметрах для отображения в результатах
            const_cast<EditInRangeCommand*>(cmd)->m_path = cmdPath;
            const_cast<EditInRangeCommand*>(cmd)->m_params["path"] = cmdPath;
        }
        if (cmdPath != path) {
            if (err) *err = "EditInRangeCommand batch must target the same file";
            return false;
        }
    }

    QStringList lines;
    if (!loadFileLines(path, lines, err))
        return false;

    QList<EditInRangeCommand*> sorted = commands;
    std::sort(sorted.begin(), sorted.end(),
              [](const EditInRangeCommand* a, const EditInRangeCommand* b){
                  return a->startLine() > b->startLine();
              });

    for (EditInRangeCommand* cmd : sorted) {
        if (!cmd->applyToLines(lines, err))
            return false;
    }

    return saveFileLines(path, lines, devAss, err);
}

bool EditInRangeCommand::applyToLines(QStringList& lines, QString* err) const
{
    if (m_startLine < 1 || m_endLine < m_startLine) {
        if (err) *err = "Invalid line range for EditInRangeCommand";
        return false;
    }

    const int startIndex = m_startLine - 1;
    const int endIndex = m_endLine - 1;
    if (startIndex >= lines.size() || endIndex >= lines.size()) {
        if (err) *err = "Line range exceeds file length";
        return false;
    }

    const int removeCount = endIndex - startIndex + 1;
    for (int i = 0; i < removeCount; ++i)
        lines.removeAt(startIndex);
    for (int i = 0; i < m_newLines.size(); ++i)
        lines.insert(startIndex + i, m_newLines.at(i));
    return true;
}

bool EditInRangeCommand::loadFileLines(const QString& path, QStringList& out, QString* err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = QString("Cannot open %1 for reading").arg(path);
        return false;
    }
    QTextStream in(&f);
    while (!in.atEnd())
        out << in.readLine();
    f.close();
    return true;
}

bool EditInRangeCommand::saveFileLines(const QString& path,
                                       const QStringList& lines,
                                       DevAssManager* devAss,
                                       QString* err)
{
    // Нормализуем путь: если путь относительный, делаем его относительно корневой директории проекта
    QString normalizedPath = path;
    if (devAss && !QFileInfo(path).isAbsolute()) {
        normalizedPath = QDir(devAss->projectRoot().absolutePath()).filePath(path);
    }

    // Создаем родительские директории, если они не существуют
    QDir dir = QFileInfo(normalizedPath).absoluteDir();
    if (!dir.exists() && !dir.mkpath(".")) {
        if (err) *err = QString("Cannot create directory for file: %1").arg(dir.path());
        return false;
    }

    QFile f(normalizedPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (err) *err = QString("Cannot open %1 for writing").arg(normalizedPath);
        return false;
    }

    QTextStream out(&f);
    for (int i = 0; i < lines.size(); ++i)
        out << lines.at(i) << '\n';
    f.close();

    if (devAss) {
        devAss->updateFileDescription(normalizedPath,
                                      QDateTime::currentDateTime(),
                                      true);
        devAss->save();
    }
    return true;
}

QJsonObject EditInRangeCommand::toJson() const
{
    QJsonObject obj;
    obj["command"]     = name();
    obj["params"]      = m_params;
    obj["description"] = description();
    return obj;
}
