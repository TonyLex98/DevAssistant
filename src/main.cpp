#include <QApplication>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <QTranslator>

#include "command/CommandConfirmDialog.h"
#include "command/CommandPolicy.h"
#include "command/CommandParser.h"
#include "command/CommandRegistry.h"
#include "command/CommandDocs.h"
#include "ui/MainWindow.h"
#include "RagWidget.h"

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
    // A relative --out is taken from the current folder; without --out the file goes to temp.
    QString outPath = argValue(argc, argv, "--out").trimmed();
    if (outPath.isEmpty()) {
        const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        outPath = QDir(tempDir).absoluteFilePath("DevAssistantQt_command_docs.txt");
    }
    outPath = QFileInfo(outPath).absoluteFilePath();
    QDir().mkpath(QFileInfo(outPath).absolutePath());
    QFile f(outPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream(stderr) << "Cannot write " << QDir::toNativeSeparators(outPath) << ": " << f.errorString() << "\n";
        return 1;
    }

    QTextStream o(stdout, QIODevice::WriteOnly);
    QTextStream fo(&f);
    fo.setCodec("UTF-8");

    const auto all = CommandRegistry::all();
    if (hasArg(argc, argv, "--markdown")) {
        // docs/COMMANDS.md: generated from the same descriptions the model is given.
        QString md = QStringLiteral(
            "# Command reference\n\n"
            "The model answers with JSON commands; DevAssistant checks each one (project folder, the step's "
            "allowed commands), shows the ones that change something for confirmation, and runs them. "
            "This file is generated from the program: `DevAssistantQt.exe --print-command-docs --markdown --out COMMANDS.md`.\n\n"
            "| Command | Risk | What it does |\n|---|---|---|\n");
        QString details;
        for (Command* c : all) {
            const CommandDoc doc = CommandDocs::doc(c->name());
            const QString risk = CommandPolicy::riskLabel(CommandPolicy::riskOf(c->name()));
            md += QStringLiteral("| [`%1`](#%2) | %3 | %4 |\n")
                    .arg(doc.name, doc.name.toLower(), risk, QString(doc.summary).replace('|', "\\|"));
            details += QStringLiteral("\n## %1\n\n%2\n\n").arg(doc.name, doc.summary);
            if (!doc.params.isEmpty()) {
                details += QStringLiteral("| Parameter | Type | Required | Description |\n|---|---|---|---|\n");
                for (const CommandParamDoc& p : doc.params)
                    details += QStringLiteral("| `%1` | %2 | %3 | %4 |\n")
                            .arg(p.name, p.type, p.required ? QStringLiteral("yes") : QStringLiteral("no"),
                                 QString(p.description).replace('|', "\\|"));
                details += "\n";
            }
            for (const QString& note : doc.notes)
                details += QStringLiteral("- %1\n").arg(note);
            if (!doc.notes.isEmpty())
                details += "\n";
            details += QStringLiteral("```json\n%1\n```\n")
                    .arg(QString::fromUtf8(QJsonDocument(doc.example).toJson(QJsonDocument::Indented)).trimmed());
            delete c;
        }
        md += details;
        o << md;
        if (f.isOpen())
            fo << md;
    } else {
        for (Command* c : all) {
            const CommandDoc doc = CommandDocs::doc(c->name());
            const QString rendered = CommandDocs::renderForLLM(doc) + "\n\n";
            o << rendered;
            if (f.isOpen())
                fo << rendered;
            delete c;
        }
    }
    o.flush();
    if (f.isOpen())
        f.close();
    return 0;
}

// Interface language: --lang en|ru, else the "ui/language" setting (auto, en, ru); auto
// follows the system. The Russian translation is compiled in (translations/*.ts).
static void installTranslations(QApplication& app, const QString& requested)
{
    QString lang = requested;
    if (lang.isEmpty())
        lang = QSettings().value(QStringLiteral("ui/language"), QStringLiteral("auto")).toString();
    if (lang == QLatin1String("auto"))
        lang = QLocale::system().name().left(2);
    if (lang != QLatin1String("ru"))
        return;

    static QTranslator appTranslator;
    if (appTranslator.load(QStringLiteral(":/i18n/devassistant_ru.qm")))
        app.installTranslator(&appTranslator);
    // Qt's own strings (standard buttons, file dialogs): next to the program in a release,
    // in the Qt installation when run from a build tree.
    static QTranslator qtTranslator;
    if (qtTranslator.load(QStringLiteral("qtbase_ru"), QCoreApplication::applicationDirPath() + "/translations")
            || qtTranslator.load(QStringLiteral("qtbase_ru"), QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);
}

// --screenshots <dir>: opens the main windows, saves them as PNG and exits. CI runs it with
// QT_QPA_PLATFORM=offscreen to keep the README pictures up to date.
static void scheduleScreenshots(MainWindow& window, const QString& dir, const QString& project,
                                const QString& file)
{
    QDir().mkpath(dir);
    window.resize(1440, 880);
    if (!project.isEmpty())
        window.openProject(project);
    QTimer::singleShot(2500, &window, [&window, dir, file]() {
        if (!file.isEmpty()) {
            window.openFileInEditor(file);
            QCoreApplication::processEvents();
        }
        window.grab().save(dir + "/main.png");
        if (auto* tabs = qobject_cast<QTabWidget*>(window.centralWidget())) {
            tabs->setCurrentIndex(1);
            QCoreApplication::processEvents();
            window.grab().save(dir + "/settings.png");
            tabs->setCurrentIndex(0);
        }

        // The confirmation dialog with a typical batch, as the model would send it.
        const QList<QJsonObject> sample = {
            QJsonObject{{"command", "OPEN_FILE_REGION"},
                        {"params", QJsonObject{{"path", "src/core/ContextManager.cpp"}, {"startLine", 80}, {"endLine", 140}}}},
            QJsonObject{{"command", "EDIT_IN_RANGE"},
                        {"params", QJsonObject{{"path", "src/core/ContextManager.cpp"}, {"startLine", 112}, {"endLine", 114},
                                               {"content", "if (path.trimmed().isEmpty())\n    return QString();\n"}}}},
            QJsonObject{{"command", "RUN_SHELL"},
                        {"params", QJsonObject{{"command", "cmake --build build --target tests && ctest --test-dir build"}}}},
            QJsonObject{{"command", "DELETE_FILE"}, {"params", QJsonObject{{"path", "build/old_report.txt"}}}},
        };
        QList<Command*> commands;
        for (const QJsonObject& o : sample) {
            if (Command* c = CommandRegistry::create(o))
                commands << c;
        }
        {
            CommandConfirmDialog dialog(commands, QStringLiteral("C:/Projects/demo"));
            dialog.resize(900, 330);
            dialog.show();
            QCoreApplication::processEvents();
            dialog.grab().save(dir + "/confirm.png");
        }
        qDeleteAll(commands);

        Rag::RagWidget documents;
        documents.setFolder(QStringLiteral("C:/Projects/demo"));
        documents.show();
        QCoreApplication::processEvents();
        documents.grab().save(dir + "/documents.png");

        QCoreApplication::quit();
    });
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
    installTranslations(app, argValue(argc, argv, "--lang"));

    // The stylesheet is compiled into the program (src/resources/resources.qrc). A styles.qss
    // next to the executable overrides it, for trying out a theme without rebuilding.
    {
        QFile styleFile(QDir(QCoreApplication::applicationDirPath()).filePath("styles.qss"));
        if (!styleFile.exists())
            styleFile.setFileName(QStringLiteral(":/styles.qss"));
        if (styleFile.open(QFile::ReadOnly | QFile::Text))
            app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }

    qRegisterMetaType<QList<int>>();

    MainWindow w;
    w.show();
    const QString screenshotDir = argValue(argc, argv, "--screenshots");
    if (!screenshotDir.isEmpty())
        scheduleScreenshots(w, screenshotDir, argValue(argc, argv, "--project"), argValue(argc, argv, "--open"));
    return app.exec();
}

