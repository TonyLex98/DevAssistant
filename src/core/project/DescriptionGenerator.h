#pragma once
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

class DevAssManager;
class ModelClient;
class ModelReply;

// Writes a short description of each project file with the model, into
// <project>/DevelopersAssistant/<file>.dadescr, one file after another without blocking.
// The prompts can include the descriptions ({{FILE_DESCRIPTIONS}}).
class DescriptionGenerator : public QObject
{
    Q_OBJECT
public:
    DescriptionGenerator(ModelClient& model, DevAssManager& files, QObject* parent = nullptr);

    void start(const QStringList& absolutePaths);
    void cancel();
    bool isRunning() const { return m_index < m_files.size(); }

    // The prompt for one file (public for tests).
    static QString prompt(const QString& fileContent);

signals:
    void progress(int done, int total, const QString& file, bool described);
    void finished(int described, int failed);

private:
    void next();
    bool save(const QString& path, const QString& description);

    ModelClient& m_model;
    DevAssManager& m_devAss;
    QStringList m_files;
    int m_index = 0;
    int m_described = 0;
    int m_failed = 0;
    QPointer<ModelReply> m_reply;
};
