#include "core/project/DescriptionGenerator.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include "core/model/ModelClient.h"
#include "core/project/DevAssManager.h"
#include "core/util/Logging.h"

DescriptionGenerator::DescriptionGenerator(ModelClient& model, DevAssManager& files, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_devAss(files)
{
}

QString DescriptionGenerator::prompt(const QString& fileContent)
{
    return QStringLiteral(
        "Write a formal but very short description of the file contents. Write in English and follow the format below strictly.\n"
        "\n"
        "If the file contains code, analyze it statically and output Markdown:\n"
        "## Purpose\n"
        "- 1-2 sentences on what the file does.\n"
        "\n"
        "## Entities\n"
        "A table (Name | Kind {class/struct/enum/namespace} | Purpose in 1 sentence).\n"
        "\n"
        "## Methods by class\n"
        "For each class, list its methods as lines like:\n"
        "- Class::method(params) - short purpose; calls: A::x, B::y; modifies: fields; throws: exceptions; signals/slots: ...\n"
        "\n"
        "## Interactions\n"
        "A list of \"source -> target\" links with an explanation (when/why), for example:\n"
        "- A::foo -> B::bar - while handling the event ...\n"
        "If there are no explicit calls, say so.\n"
        "\n"
        "## Dependencies\n"
        "External libraries/Qt classes and what they are used for.\n"
        "\n"
        "## Public API\n"
        "Key entry points (a short list).\n"
        "\n"
        "If this is NOT code, give a concise 2-3 sentence summary.\n"
        "Do not invent facts; rely only on the text below.\n"
        "\n"
        "-----\n"
        "%1").arg(fileContent);
}

void DescriptionGenerator::start(const QStringList& absolutePaths)
{
    cancel();
    m_files = absolutePaths;
    m_index = 0;
    m_described = 0;
    m_failed = 0;
    next();
}

void DescriptionGenerator::cancel()
{
    m_index = m_files.size();
    if (m_reply)
        m_reply->abort();
}

void DescriptionGenerator::next()
{
    if (m_index >= m_files.size()) {
        emit finished(m_described, m_failed);
        return;
    }
    const QString path = m_files.at(m_index);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ++m_failed;
        emit progress(++m_index, m_files.size(), path, false);
        next();
        return;
    }
    ModelRequest request;
    request.user = prompt(QString::fromUtf8(file.readAll()));
    request.timeoutMs = 300000;  // a local model may take minutes for a large file
    ModelReply* reply = m_model.send(request);
    m_reply = reply;
    connect(reply, &ModelReply::finished, this, [this, reply, path]() {
        reply->deleteLater();
        if (reply->isCancelled())
            return;
        const QString text = reply->text().trimmed();
        const bool ok = !reply->hasError() && !text.isEmpty() && save(path, text);
        if (!ok)
            qCWarning(logUtils) << "No description for" << path << reply->errorString();
        if (ok)
            ++m_described;
        else
            ++m_failed;
        emit progress(++m_index, m_files.size(), path, ok);
        next();
    });
}

bool DescriptionGenerator::save(const QString& path, const QString& description)
{
    const QString rel = QDir(m_devAss.projectRoot().absolutePath()).relativeFilePath(path);
    const QString descrPath = m_devAss.devAssDir().filePath(rel + QStringLiteral(".dadescr"));
    QDir().mkpath(QFileInfo(descrPath).absolutePath());
    QFile f(descrPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    f.write(description.toUtf8());
    f.close();
    m_devAss.updateFileDescription(path, QDateTime::currentDateTime(), true);
    m_devAss.save();
    return true;
}
