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
        ? QString("Replaces a specific range of lines in a file. "
                  "IMPORTANT: Use the line numbers from OPEN_FILE or OPEN_FILE_REGION output! "
                  "The 'startLine' and 'endLine' must match the line numbers you see in the file content. "
                  "Example: { \"command\": \"EDIT_IN_RANGE\", \"params\": { \"path\": \"src/main.cpp\", \"startLine\": 10, \"endLine\": 12, \"replacementLines\": [ { \"line\": 10, \"text\": \"int a = 1;\" } ] } }")
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
    if (path.trimmed().isEmpty()) {
        if (err) *err = "EDIT_IN_RANGE: path is empty";
        return false;
    }
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

    QByteArray outBytes;
    outBytes.reserve(1024);
    for (int i = 0; i < lines.size(); ++i) {
        outBytes += lines.at(i).toUtf8();
        outBytes += '\n';
    }

    if (devAss) {
        QList<Change> changes;
        changes << Change::replaceInFile(normalizedPath, outBytes);
        return devAss->applyChanges(changes, err);
    }

    QFile f(normalizedPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (err) *err = QString("Cannot open %1 for writing").arg(normalizedPath);
        return false;
    }
    f.write(outBytes);
    f.close();
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

QString EditInRangeCommand::getResult() const
{
    const bool ok = wasSuccessful();
    QString contentPreview = m_newLines.join("\n");
    const int maxPreview = 400;
    if (contentPreview.size() > maxPreview) {
        contentPreview = contentPreview.left(maxPreview) + "...";
    }

    QString result = QString("EDIT_IN_RANGE: %1 [%2-%3]\n[status]\n%4\n[replacement]\n%5\nexitCode=%6")
            .arg(m_params.value("path").toString())
            .arg(m_params.value("startLine").toInt())
            .arg(m_params.value("endLine").toInt())
            .arg(ok ? "Lines updated successfully" : "Failed to update lines")
            .arg(contentPreview.isEmpty() ? "(empty replacement)" : contentPreview)
            .arg(ok ? 0 : 1);

    if (!getErrorMessage().isEmpty()) {
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    }
    return result;
}
