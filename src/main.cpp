#include <QApplication>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

#include "command/CommandParser.h"
#include "command/CommandRegistry.h"
#include "command/CommandDocs.h"
#include "ui/MainWindow.h"

static bool hasArg(int argc, char** argv, const char* arg)
{
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QString::fromLatin1(arg))
            return true;
    }
    return false;
}

static QString argValue(int argc, char** argv, const char* arg)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QString::fromLatin1(arg)) {
            return QString::fromLocal8Bit(argv[i + 1]);
        }
    }
    return QString();
}

static int runParseCommandsMode(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream in(stdin, QIODevice::ReadOnly);
    const QString input = in.readAll();

    QVector<Command*> cmds = CommandParser::parse(input.toUtf8());
    QJsonArray out;
    for (Command* c : cmds) {
        out.append(c->toJson());
        delete c;
    }

    const QByteArray payload = QJsonDocument(out).toJson(QJsonDocument::Indented);

    // Note: the GUI binary is linked as Windows subsystem, so stdout may be unavailable.
    // Always write a file so tests can read results reliably.
    QString outPath = argValue(argc, argv, "--out").trimmed();
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (outPath.isEmpty()) {
        outPath = QDir(tempDir).absoluteFilePath("DevAssistantQt_parsed_commands.json");
    } else {
        QFileInfo info(outPath);
        if (info.isRelative()) {
            outPath = QDir(tempDir).absoluteFilePath(outPath);
        }
    }

    QFile f(outPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(payload);
        f.close();
    }

    QTextStream o(stdout, QIODevice::WriteOnly);
    o << QString::fromUtf8(payload);
    o.flush();

    return out.isEmpty() ? 2 : 0;
}

static int runPrintCommandDocsMode(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QString outPath = argValue(argc, argv, "--out").trimmed();
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (outPath.isEmpty()) {
        outPath = QDir(tempDir).absoluteFilePath("DevAssistantQt_command_docs.txt");
    } else {
        QFileInfo info(outPath);
        if (info.isRelative()) {
            outPath = QDir(tempDir).absoluteFilePath(outPath);
        }
    }
    QFile f(outPath);
    f.open(QIODevice::WriteOnly | QIODevice::Truncate);

    QTextStream o(stdout, QIODevice::WriteOnly);
    QTextStream fo(&f);

    const auto all = CommandRegistry::all();
    for (Command* c : all) {
        const CommandDoc doc = CommandDocs::doc(c->name());
        const QString rendered = CommandDocs::renderForLLM(doc) + "\n\n";
        o << rendered;
        if (f.isOpen())
            fo << rendered;
        delete c;
    }
    o.flush();
    if (f.isOpen())
        f.close();
    return 0;
}

int main(int argc, char** argv)
{
    QCoreApplication::setOrganizationName("Anton");
    QCoreApplication::setOrganizationDomain("devassistant.local");
    QCoreApplication::setApplicationName("DevAssistantQt");

    if (hasArg(argc, argv, "--parse-commands"))
        return runParseCommandsMode(argc, argv);
    if (hasArg(argc, argv, "--print-command-docs"))
        return runPrintCommandDocsMode(argc, argv);

    QApplication app(argc, argv);

    // Load global stylesheet from .qss file
    {
        QString stylePath = QCoreApplication::applicationDirPath() + "/../src/resources/styles.qss";
        QFile styleFile(stylePath);
        if (!styleFile.exists()) {
            // Fallback: try relative to executable
            stylePath = QDir(QCoreApplication::applicationDirPath()).filePath("styles.qss");
            styleFile.setFileName(stylePath);
        }
        if (styleFile.exists() && styleFile.open(QFile::ReadOnly | QFile::Text)) {
            app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
            styleFile.close();
        } else {
            // Hardcoded fallback if file not found
            app.setStyleSheet(
                "QMainWindow, QDialog, QWidget { background: #f2fbf9; color: #0f172a; font: 10pt \"Segoe UI Variable\"; }"
                "QLabel { color: #0f172a; }"
                "QListWidget, QTreeWidget, QTreeView, QTableView, QPlainTextEdit, QTextEdit, QTextBrowser, QGraphicsView {"
                "  background: #ffffff; border: 1px solid #bfeee6; border-radius: 6px; }"
            );
        }
    }

    qRegisterMetaType<QList<int>>();

    MainWindow w;
    w.show();
    return app.exec();
}


/*
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    VoskWidget w;
    w.show();

    return app.exec();
}
*/
