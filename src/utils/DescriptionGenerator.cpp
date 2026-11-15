// DescriptionGenerator.cpp
#include "DescriptionGenerator.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include "utils/Logging.h"

DescriptionGenerator::DescriptionGenerator(OllamaAdapter* model,
                                         DevAssManager* devAss,
                                         QObject* parent)
    : QObject(parent), m_model(model), m_devAss(devAss)
{
}

QString DescriptionGenerator::relativePath(const QString& absPath)
{
    return QDir(m_devAss->projectRoot().absolutePath())
                .relativeFilePath(absPath);
}

QString DescriptionGenerator::createPrompt(const QString& content)
{
    return QString(
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
        "%1"
    ).arg(content);
}


QString DescriptionGenerator::writeDescription(const QString& relPath,
                                              const QString& description)
{
    // *.dadescr path inside the DevAss folder
    QString descrPath = m_devAss->devAssDir().filePath(relPath + ".dadescr");

    QDir().mkpath(QFileInfo(descrPath).absolutePath());

    QFile f(descrPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(logUtils) << "Failed to write the descriptor:" << descrPath;
        return QString();
    }
    f.write(description.toUtf8());
    f.close();

    return descrPath;
}

QString DescriptionGenerator::generateAndSave(const QString& filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile())
        return QString();

    // --- 1. Is the description up to date? ---
    FileDescription* fd = m_devAss->findFileDescription(filePath);
    QDateTime fileMod = fi.lastModified();
    if (fd && fd->isDescriptionUpToDate && fileMod <= fd->lastModTime)
        return QString(); // already up to date, nothing to do

    // --- 2. Read the file ---
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(logUtils) << "Failed to open the file:" << filePath;
        return QString();
    }
    QString content = QString::fromUtf8(f.readAll());
    f.close();

    // --- 3. Generate with the model ---
    QString prompt = createPrompt(content);
    QString description = m_model->generate(prompt);
    if (description.isEmpty()) {
        qCWarning(logUtils) << "The LLM returned an empty description for:" << filePath;
        return QString();
    }

    // --- 4. Write the *.dadescr file ---
    QString relPath = relativePath(filePath);
    QString descrPath = writeDescription(relPath, description);
    if (descrPath.isEmpty())
        return QString(); // write error

    // --- 5. Update .dapro ---
    m_devAss->updateFileDescription(filePath,
                                    QDateTime::currentDateTime(),
                                    true);
    m_devAss->save();

    return description;
}
