#include "RAGProcessor.h"
#include "Database.h"
#include "Encoder.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <QTextStream>
#include <QRegularExpression>
#include <QDir>
#include <QProcess>

namespace Rag
{

static const QStringList supportedExtensions = {"pdf", "txt", "docx", "cpp", "c", "h", "hpp", "py", "js", "ts", "java", "cs", "go", "rust", "rb", "php", "html", "css", "json", "xml", "yaml", "yml", "md", "sql", "sh", "bat", "ini", "cfg", "config"};

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
    // For now, just set initialized to true
    // In a real implementation, this would check Ollama connection
    mInitialized = true;
}

void RAGProcessor::initWithOllamaAdapter(OllamaAdapter* adapter)
{
    if (mEncoder) {
        mEncoder->setOllamaAdapter(adapter);
    }
    mInitialized = true;
    emit statusUpdate("RAGProcessor initialized with main Ollama adapter");
}

void RAGProcessor::indexFolder(const QString &rootPath)
{
    if (!mInitialized) {
        qDebug() << "RAGProcessor not initialized";
        return;
    }
    
    emit statusUpdate("Starting indexing of folder: " + rootPath);
    
    // Run indexing in a separate thread to keep UI responsive
    QMetaObject::invokeMethod(this, [this, rootPath = rootPath]() {
        int totalFiles = 0;
        int processed = 0;
        QVector<Fragment> fragments;

        // First, count files
        QDirIterator it_count(rootPath, supportedExtensions, QDir::Files, QDirIterator::Subdirectories);
        while (it_count.hasNext()) {
            ++totalFiles;
            it_count.next();
        }

        emit progress(0, totalFiles);

        // Create a new iterator for the real work since QDirIterator doesn't have reset()
        QDirIterator it_process(rootPath, supportedExtensions, QDir::Files, QDirIterator::Subdirectories);
        while (it_process.hasNext()) {
            it_process.next();
            const QString filePath = it_process.filePath();
            const QFileInfo fi(filePath);
            const QString ext = fi.suffix().toLower();
            
            QString content;
            if (ext == "pdf") {
                content = processPDF(filePath);
            } else if (ext == "txt" || ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp" || 
                      ext == "py" || ext == "js" || ext == "ts" || ext == "java" || ext == "cs" || 
                      ext == "go" || ext == "rust" || ext == "rb" || ext == "php" || ext == "html" || 
                      ext == "css" || ext == "json" || ext == "xml" || ext == "yaml" || ext == "yml" || 
                      ext == "md" || ext == "sql" || ext == "sh" || ext == "bat" || ext == "ini" || 
                      ext == "cfg" || ext == "config") {
                // Read programming and text files directly
                QFile file(filePath);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&file);
                    content = in.readAll();
                    file.close();
                }
            } else if (ext == "docx") {
                content = processDOCX(filePath);
            }
            
            if (content.isEmpty()) {
                processed++;
                emit progress(processed, totalFiles);
                continue;
            }
            
            // Split into 2K character fragments. Here we split by code blocks intelligently.
            const QStringList parts = splitCodeIntoFragments(content, 2000);
            int partIdx = 0;
            for (const QString &part : parts) {
                Fragment frag;
                frag.filePath = filePath;
                frag.paragraphNumber = partIdx;
                frag.text = part.trimmed();
                
                if (!frag.text.isEmpty()) {
                    emit statusUpdate(QString("Processing fragment %1 of %2 from %3").arg(partIdx + 1).arg(parts.size()).arg(QFileInfo(filePath).fileName()));
                    frag.embedding = mEncoder->embed(frag.text);
                    fragments.push_back(frag);
                }
                ++partIdx;
            }
            processed++;
            emit progress(processed, totalFiles);
        }

        // Bulk insert into DB
        mDb->insertFragments(fragments);
        emit statusUpdate("Indexing completed. Processed " + QString::number(fragments.size()) + " fragments.");
        emit indexingFinished();
    });
}

// Helper to split code into sensible fragments (by functions, classes, etc.)
QStringList RAGProcessor::splitCodeIntoFragments(const QString &content, int maxFragmentSize)
{
    QStringList fragments;
    
    // Try to split by functions/classes when possible, otherwise by lines
    QStringList lines = content.split('\n');
    
    QString currentFragment;
    for (const QString &line : lines) {
        if (currentFragment.length() + line.length() + 1 > maxFragmentSize && !currentFragment.isEmpty()) {
            // If current fragment is too large, start a new one
            if (!currentFragment.trimmed().isEmpty()) {
                fragments.append(currentFragment);
            }
            currentFragment = line + "\n";
        } else {
            currentFragment += line + "\n";
        }
    }
    
    // Add the last fragment
    if (!currentFragment.trimmed().isEmpty()) {
        fragments.append(currentFragment);
    }
    
    return fragments;
}

void RAGProcessor::ask(const QString &question)
{
    if (!mInitialized) {
        qDebug() << "RAGProcessor not initialized";
        return;
    }
    
    emit statusUpdate("Processing query: " + question.left(50) + "...");
    
    QMetaObject::invokeMethod(this, [this, question = question]() {
        emit statusUpdate("Generating embedding for query...");
        QVector<float> queryVec = mEncoder->embed(question);
        emit statusUpdate("Searching for similar fragments...");
        QVector<Fragment> nearest = mDb->searchSimilar(queryVec, 5);
        
        // Concatenate fragments and feed into LLM.
        QString context;
        for (const auto &f : nearest) {
            context += QString("File: %1 (Line %2)\n%3\n\n")
                          .arg(QFileInfo(f.filePath).fileName())
                          .arg(f.paragraphNumber)
                          .arg(f.text);
        }
        
        emit statusUpdate("Generating answer using LLM...");
        QString answer = mEncoder->generateAnswer(question, context);
        emit statusUpdate("Answer generation completed.");
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

void RAGProcessor::indexSpecificFiles(const QStringList &filePaths)
{
    if (!mInitialized) {
        qDebug() << "RAGProcessor not initialized";
        return;
    }
    
    emit statusUpdate("Starting indexing of specific files: " + QString::number(filePaths.size()) + " files");
    
    // Run indexing in a separate thread to keep UI responsive
    QMetaObject::invokeMethod(this, [this, filePaths = filePaths]() {
        int totalFiles = filePaths.size();
        int processed = 0;
        QVector<Fragment> fragments;

        emit progress(0, totalFiles);
        
        for (const QString &filePath : filePaths) {
            const QFileInfo fi(filePath);
            const QString ext = fi.suffix().toLower();
            
            QString content;
            if (ext == "pdf") {
                content = processPDF(filePath);
            } else if (ext == "txt" || ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp" || 
                      ext == "py" || ext == "js" || ext == "ts" || ext == "java" || ext == "cs" || 
                      ext == "go" || ext == "rust" || ext == "rb" || ext == "php" || ext == "html" || 
                      ext == "css" || ext == "json" || ext == "xml" || ext == "yaml" || ext == "yml" || 
                      ext == "md" || ext == "sql" || ext == "sh" || ext == "bat" || ext == "ini" || 
                      ext == "cfg" || ext == "config") {
                // Read programming and text files directly
                QFile file(filePath);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&file);
                    content = in.readAll();
                    file.close();
                }
            } else if (ext == "docx") {
                content = processDOCX(filePath);
            }
            
            if (content.isEmpty()) {
                processed++;
                emit progress(processed, totalFiles);
                continue;
            }
            
            // Split into 2K character fragments
            const QStringList parts = splitCodeIntoFragments(content, 2000);
            int partIdx = 0;
            for (const QString &part : parts) {
                Fragment frag;
                frag.filePath = filePath;
                frag.paragraphNumber = partIdx;
                frag.text = part.trimmed();
                
                if (!frag.text.isEmpty()) {
                    emit statusUpdate(QString("Processing fragment %1 of %2 from %3").arg(partIdx + 1).arg(parts.size()).arg(fi.fileName()));
                    frag.embedding = mEncoder->embed(frag.text);
                    fragments.push_back(frag);
                }
                ++partIdx;
            }
            processed++;
            emit progress(processed, totalFiles);
        }

        // Bulk insert into DB
        mDb->insertFragments(fragments);
        emit statusUpdate("Indexing of specific files completed. Processed " + QString::number(fragments.size()) + " fragments.");
        emit indexingFinished();
    });
}

QVector<Fragment> RAGProcessor::search(const QString &query, int limit)
{
    if (!mInitialized) {
        qDebug() << "RAGProcessor not initialized";
        return QVector<Fragment>();
    }
    
    emit statusUpdate("Searching for: " + query.left(50) + "...");
    
    // Generate embedding for the query
    QVector<float> queryVec = mEncoder->embed(query);
    emit statusUpdate("Searching for similar fragments...");
    
    // Search for similar fragments in the database
    QVector<Fragment> results = mDb->searchSimilar(queryVec, limit);
    
    emit statusUpdate("Search completed. Found " + QString::number(results.size()) + " results.");
    
    return results;
}

} // namespace Rag