#include "PythonScripts.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>

#include <memory>

namespace PythonScripts {

QString pythonProgram()
{
    const QString configured = QSettings().value(QStringLiteral("speech/python")).toString().trimmed();
    return configured.isEmpty() ? QStringLiteral("python") : configured;
}

QString findScript(const QString& fileName)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    for (const QString& dir : {appDir + "/scripts", appDir + "/../scripts", appDir + "/../../scripts"}) {
        const QFileInfo fi(dir + "/" + fileName);
        if (fi.exists())
            return fi.absoluteFilePath();
    }
    return QString();
}

void checkPython(QObject* context, std::function<void(bool, const QString&)> done)
{
    auto* process = new QProcess(context);
    auto finished = std::make_shared<bool>(false);
    const auto finish = [process, done, finished](bool ok, const QString& error) {
        if (*finished)
            return;
        *finished = true;
        done(ok, error);
        process->deleteLater();
    };

    QObject::connect(process, &QProcess::errorOccurred, context, [finish](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            finish(false, QCoreApplication::translate("PythonScripts",
                    "Python was not found. Install Python 3 and add it to PATH, or set its path in Settings."));
        }
    });
    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), context,
                     [finish](int code, QProcess::ExitStatus status) {
        if (status == QProcess::NormalExit && code == 0)
            finish(true, QString());
        else
            finish(false, QCoreApplication::translate("PythonScripts",
                    "Python did not run (exit code %1). On Windows this is often the Microsoft Store "
                    "placeholder: install Python from python.org.").arg(code));
    });
    process->start(pythonProgram(), {QStringLiteral("--version")});
}

} // namespace PythonScripts
