#include "RAGProcessor.h"
#include "Database.h"
#include "Encoder.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QFile>
#include <QDebug>

namespace Rag
{

constexpr QStringList supportedExtensions = {"pdf", "txt", "docx"};

RAGProcessor::RAGProcessor(QObject *parent) : QObject(parent)
{
    mDb = new Database();
    mEncoder = new Encoder();
    mInitialized = false;
    mThreadPool.setMaxThreadCount(2);
}

RAGProcessor::~RAGProcessor()
{
    delete mEncoder;
    delete mDb;
}

// -------------
void RAGProcessor::init()
{
    if (mInitialized) return;
    // 1. Проверка версии ollama
    QProcess proc;
    proc.start("ollama", {"--version"});
    if (!proc.waitForFinished(5000)) {
        qWarning() << "Failed to run ollama";
        return;
    }
    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    QRegularExpression re("^ollama\\s+(\S+)");
    auto match = re.match(output);
    if (!match.hasMatch() || match.captured(1) != "0.11.10") {
        qWarning() << "ollama 0.11.10 not found. Please install it.";
        return;
    }

    // 2. Загрузить модель embeddinggemma:latest
    QProcess pull;
    pull.start("ollama", {"pull", "embeddinggemma:latest"});
    pull.waitForFinished();
    qDebug() << "Model load result:" << QString::fromUtf8(pull.readAllStandardOutput());

    mInitialized = true;
}
void RAGProcessor::indexFolder(const QString &rootPath)
{
    // Run indexing in a separate thread to keep UI responsive
    QMetaObject::invokeMethod(this, [this, rootPath = rootPath]() {
        int totalFiles = 0;
        int processed = 0;
        QVector<Fragment> fragments;

        // First, count files
        QDirIterator it(rootPath, supportedExtensions, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) { ++totalFiles; it.next(); }

        it.reset(); // restart iterator for real work
        while (it.hasNext()) {
            it.next();
            const QString filePath = it.filePath();
            const QFileInfo fi(filePath);
            const QString ext = fi.suffix().toLower();
            QString content;
            if (ext == "pdf") {
                content = processPDF(filePath);
            } else if (ext == "txt") {
                content = QString::fromUtf8(QFile::readAll(filePath));
            } else if (ext == "docx") {
                content = processDOCX(filePath);
            }
            if (content.isEmpty()) {
                processed++;
                emit progress(processed, totalFiles);
                continue;
            }
            // Split into 2K token fragments. Here we simply split by paragraphs.
            const QStringList paragraphs = content.split("\n\n", Qt::SkipEmptyParts);
            int paraIdx = 0;
            for (const QString &para : paragraphs) {
                for (int offset = 0; offset < para.length(); offset += 2000) { // naive split
                    Fragment frag;
                    frag.filePath = filePath;
                    frag.paragraphNumber = paraIdx;
                    frag.text = para.mid(offset, 2000);
                    frag.embedding = mEncoder->embed(frag.text);
                    fragments.push_back(frag);
                }
                ++paraIdx;
            }
            processed++;
            emit progress(processed, totalFiles);
        }

        // Bulk insert into DB
        mDb->insertFragments(fragments);
        emit indexingFinished();
    });
}

void RAGProcessor::ask(const QString &question)
{
    QMetaObject::invokeMethod(this, [this, question = question]() {
        QVector<float> queryVec = mEncoder->embed(question);
        QVector<Fragment> nearest = mDb->searchSimilar(queryVec, 5);
        // Concatenate fragments and feed into LLM.
        QString context;
        for (const auto &f : nearest) context += f.text + "\n";
        QString answer = mEncoder->generateAnswer(question, context);
        emit answered(answer);
    });
}

// Helper: run external pdftotext and capture output
QString RAGProcessor::processPDF(const QString &filePath) const
{
    // Use pdftotext -layout -q to extract text. Requires poppler-utils.
    QProcess p;
    p.start("pdftotext", {filePath, "-"});
    if (!p.waitForFinished(30000)) return {}; // timeout
    return QString::fromUtf8(p.readAllStandardOutput());
}

// Helper for docx via docx2txt
QString RAGProcessor::processDOCX(const QString &filePath) const
{
    QProcess p;
    p.start("docx2txt", {filePath, "-"});
    if (!p.waitForFinished(30000)) return {};
    return QString::fromUtf8(p.readAllStandardOutput());
}

// --- init -------------------------------------------------------------------
void Rag::RAGProcessor::init()
{
    if (mInitialized) return;
    // Verify Ollama version
    QProcess p;
    p.start("ollama", {"--version"});
    if (!p.waitForFinished(5000)) {
        qWarning() << "Cannot start Ollama";
        return;
    }
    QString version = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    if (!version.startsWith("0.11.10")) {
        qWarning() << "Expected Ollama 0.11.10 but got" << version;
        // You may want to abort or try upgrade.
    }
    // Pull embeddinggemma model
    p.start("ollama", {"pull", "embeddinggemma:latest"});
    if (!p.waitForFinished(600000)) {
        qWarning() << "Ollama pull timeout";
    }
    mInitialized = true;
}

} // namespace Rag
