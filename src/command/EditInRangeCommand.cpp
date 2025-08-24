#include "EditInRangeCommand.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

EditInRangeCommand::EditInRangeCommand(const QString& path,
                                       int startLine, int endLine,
                                       const QByteArray& content,
                                       const QString& descr)
{
    m_params["path"]      = path;
    m_params["startLine"] = startLine;
    m_params["endLine"]   = endLine;
    m_params["content"]   = QString::fromUtf8(content);

    m_description = descr.isEmpty()
        ? QString("Редактировать диапазон строк %1–%2 в файле «%3». Лучше переписывать файл полностью.")
              .arg(startLine).arg(endLine).arg(path)
        : descr;
}

bool EditInRangeCommand::apply(DevAssManager* devAss, QString* err)
{
    /* ---------- Чтение исходного файла ---------- */
    const QString path = m_params["path"].toString();
    const int start    = m_params["startLine"].toInt();
    const int end      = m_params["endLine"].toInt();
    const QByteArray newCont = m_params["content"].toString().toUtf8();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = "Не удалось открыть файл для чтения";
        return false;
    }
    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd())
        lines << in.readLine();
    f.close();

    if (start < 0 || end >= lines.size() || start > end) {
        if (err) *err = "Неверные номера строк";
        return false;
    }

    /* ---------- Формируем новые строки ---------- */
    QStringList newLines = QString::fromUtf8(newCont).split('\n',
                                    QString::KeepEmptyParts);

    /* ---------- Создаём новый список строк ---------- */
    QStringList merged;
    merged.reserve(lines.size() - (end - start + 1) + newLines.size());
    merged.append(lines.mid(0, start));
    merged.append(newLines);
    merged.append(lines.mid(end + 1));

    /* ---------- Записываем обновлённый файл ---------- */
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (err) *err = "Не удалось открыть файл для записи";
        return false;
    }
    QTextStream out(&f);
    for (const QString& l : qAsConst(merged))
        out << l << '\n';
    f.close();

    /* ---------- (опционально) обновляем запись в .dapro ---------- */
    if (devAss) {
        devAss->updateFileDescription(path,
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
