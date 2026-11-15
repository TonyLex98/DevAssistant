// CreateDirCommand.cpp
#include "core/commands/CreateDirCommand.h"
#include <QDir>
#include <QFileInfo>

CreateDirCommand::CreateDirCommand(const QString& path, const QString& descr)
{
    m_params["path"] = path;
    m_description = descr.isEmpty()
        ? QStringLiteral("Creates a directory (recursively). "
                         "Example: { \"command\": \"CREATE_DIR\", \"params\": { \"path\": \"src/new_folder\" } }")
        : descr;
}

static QString normalizeDirPathMaybe(DevAssManager* devAss, const QString& p)
{
    if (!devAss)
        return p;
    if (QFileInfo(p).isAbsolute())
        return QDir::cleanPath(p);
    return QDir(devAss->projectRoot().absolutePath()).filePath(p);
}

bool CreateDirCommand::apply(DevAssManager* devAss, QString* err)
{
    QString path = m_params.value("path").toString().trimmed();
    if (path.isEmpty()) {
        if (err) *err = "CREATE_DIR: path is empty";
        return false;
    }

    path = normalizeDirPathMaybe(devAss, path);
    m_params["path"] = path;

    if (devAss) {
        QList<Change> changes;
        changes << Change::createDir(path);
        return devAss->applyChanges(changes, err);
    }

    if (!QDir().mkpath(path)) {
        if (err) *err = QString("CREATE_DIR: cannot create directory: %1").arg(path);
        return false;
    }
    return true;
}

QJsonObject CreateDirCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString CreateDirCommand::getResult() const
{
    const bool ok = wasSuccessful();
    const QString path = m_params.value("path").toString();
    QString result = QString("CREATE_DIR: %1\n[status]\n%2\nexitCode=%3")
                         .arg(path)
                         .arg(ok ? "Directory created successfully" : "Directory creation failed")
                         .arg(ok ? 0 : 1);
    if (!getErrorMessage().isEmpty())
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    return result;
}

