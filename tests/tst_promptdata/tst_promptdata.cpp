#include <QtTest>
#include <QDir>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QStandardPaths>

#include "CommandRegistry.h"
#include "PromptSequence.h"
#include "PromptStep.h"
#include "SolutionTemplate.h"

namespace {

QString repoDir()
{
    return QString::fromUtf8(DEVASS_REPO_DIR);
}

// Files shipped with the repository whose names match `pattern`; build output and the
// fetched tree-sitter sources are skipped.
QStringList repoFiles(const QString& pattern)
{
    const QDir root(repoDir());
    QStringList out;
    QDirIterator it(root.path(), QStringList{pattern}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString rel = root.relativeFilePath(path);
        if (rel.startsWith(QLatin1String(".git/")) || rel.startsWith(QLatin1String("src/uiide/tree-sitter"))
                || rel.startsWith(QLatin1String("build")) || rel.startsWith(QLatin1String("release/"))
                || rel.startsWith(QLatin1String("debug/")))
            continue;
        out << path;
    }
    out.sort();
    return out;
}

QJsonObject readJsonObject(const QString& path, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("cannot open %1").arg(path);
        return QJsonObject();
    }
    // Read exactly as PromptSequenceManager and SolutionTemplateManager do.
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError)
        *error = QStringLiteral("%1: %2 at offset %3").arg(path, parseError.errorString()).arg(parseError.offset);
    else if (!doc.isObject())
        *error = QStringLiteral("%1: not a JSON object").arg(path);
    return doc.object();
}

QSet<QString> registeredNames()
{
    QSet<QString> names;
    for (Command* c : CommandRegistry::all()) {
        names.insert(c->name());
        delete c;
    }
    return names;
}

QJsonObject parseObject(const QByteArray& json)
{
    return QJsonDocument::fromJson(json).object();
}

} // namespace

class TestPromptData : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void stepRoundTrip();
    void stepDefaults();
    void sequenceRoundTrip();
    void sequenceSkipsInvalidSteps();
    void templateRoundTrip();
    void templateReadsLegacyConnections();
    void templatePrefersSequenceTargets();

    void repoHasDataFiles();
    void repoSequenceParses_data();
    void repoSequenceParses();
    void repoTemplateIsConsistent_data();
    void repoTemplateIsConsistent();
};

void TestPromptData::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestPromptData::stepRoundTrip()
{
    PromptStep step(QStringLiteral("Сделай: {{USER_QUERY}}"),
                    QStringList{QStringLiteral("OPEN_FILE"), QStringLiteral("EDIT_IN_RANGE")});
    step.setLoop(true);

    const PromptStep back = PromptStep::fromJson(step.toJson());
    QCOMPARE(back.tmpl(), step.tmpl());
    QCOMPARE(back.allowedCommands(), step.allowedCommands());
    QCOMPARE(back.loop(), true);
}

void TestPromptData::stepDefaults()
{
    const PromptStep step = PromptStep::fromJson(QJsonObject{{"template", "x"}});
    QCOMPARE(step.tmpl(), QStringLiteral("x"));
    QVERIFY(step.allowedCommands().isEmpty());
    QVERIFY(!step.loop());
}

void TestPromptData::sequenceRoundTrip()
{
    PromptSequence seq(QStringLiteral("Последовательность"));
    seq.setDescription(QStringLiteral("description"));
    seq.addStep(PromptStep(QStringLiteral("first"), QStringList{QStringLiteral("OPEN_FILE")}));
    PromptStep second(QStringLiteral("second"));
    second.setLoop(true);
    seq.addStep(second);

    // Through text, as it is saved to and read from a .seq.json file.
    const QByteArray saved = QJsonDocument(seq.toJson()).toJson();
    const PromptSequence back = PromptSequence::fromJson(QJsonDocument::fromJson(saved).object());
    QCOMPARE(back.name(), seq.name());
    QCOMPARE(back.description(), seq.description());
    QCOMPARE(back.steps().size(), 2);
    QCOMPARE(back.steps().at(0).tmpl(), QStringLiteral("first"));
    QCOMPARE(back.steps().at(0).allowedCommands(), QStringList{QStringLiteral("OPEN_FILE")});
    QVERIFY(!back.steps().at(0).loop());
    QCOMPARE(back.steps().at(1).tmpl(), QStringLiteral("second"));
    QVERIFY(back.steps().at(1).loop());
}

void TestPromptData::sequenceSkipsInvalidSteps()
{
    const PromptSequence seq = PromptSequence::fromJson(parseObject(
            R"({"name":"s","steps":[{"template":"a"},42,"text",{"template":"b"}]})"));
    QCOMPARE(seq.steps().size(), 2);
    QCOMPARE(seq.steps().at(1).tmpl(), QStringLiteral("b"));
}

void TestPromptData::templateRoundTrip()
{
    SolutionTemplate t(QStringLiteral("Шаблон"));
    t.setDescription(QStringLiteral("description"));
    t.setImagePath(QStringLiteral("image.png"));
    t.setMainSequencePath(QStringLiteral("main.seq.json"));
    t.addSequence(QStringLiteral("main.seq.json"));
    t.addSequence(QStringLiteral("other.seq.json"));
    t.setTargetsForSequence(QStringLiteral("main.seq.json"), QJsonArray{"other.seq.json"});
    t.setPositionForSequence(QStringLiteral("main.seq.json"), QPointF(10.5, -3));

    const QByteArray saved = QJsonDocument(t.toJson()).toJson();
    const SolutionTemplate back = SolutionTemplate::fromJson(QJsonDocument::fromJson(saved).object());
    QCOMPARE(back.name(), t.name());
    QCOMPARE(back.description(), t.description());
    QCOMPARE(back.imagePath(), t.imagePath());
    QCOMPARE(back.mainSequencePath(), t.mainSequencePath());
    QCOMPARE(back.sequences(), t.sequences());
    QCOMPARE(back.getTargetsForSequence(QStringLiteral("main.seq.json")), QJsonArray{"other.seq.json"});
    QVERIFY(back.getTargetsForSequence(QStringLiteral("other.seq.json")).isEmpty());
    QCOMPARE(back.getPositionForSequence(QStringLiteral("main.seq.json")), QPointF(10.5, -3));
}

void TestPromptData::templateReadsLegacyConnections()
{
    const SolutionTemplate t = SolutionTemplate::fromJson(parseObject(
            R"({"name":"Old","sequences":["a","b","c"],"connections":[)"
            R"({"from":"a","to":"b","type":"OPEN_SEQUENCE"},{"from":"a","to":"c"},{"from":"b","to":"a"},)"
            R"({"from":"","to":"c"},{"from":"c"}]})"));
    QCOMPARE(t.getTargetsForSequence(QStringLiteral("a")), (QJsonArray{"b", "c"}));
    QCOMPARE(t.getTargetsForSequence(QStringLiteral("b")), QJsonArray{"a"});
    QVERIFY(t.getTargetsForSequence(QStringLiteral("c")).isEmpty());
    QCOMPARE(t.getSequenceTargets().size(), 2);

    // Saving writes the current form.
    const QJsonObject saved = t.toJson();
    QVERIFY(saved.contains(QStringLiteral("sequenceTargets")));
    QVERIFY(!saved.contains(QStringLiteral("connections")));
}

void TestPromptData::templatePrefersSequenceTargets()
{
    const SolutionTemplate t = SolutionTemplate::fromJson(parseObject(
            R"({"name":"Both","sequences":["a","b","c"],"sequenceTargets":{"a":["c"]},)"
            R"("connections":[{"from":"a","to":"b"}]})"));
    QCOMPARE(t.getTargetsForSequence(QStringLiteral("a")), QJsonArray{"c"});
}

void TestPromptData::repoHasDataFiles()
{
    const QStringList sequences = repoFiles(QStringLiteral("*.seq.json"));
    QVERIFY2(sequences.size() >= 6, qPrintable(QStringLiteral("found: ") + sequences.join(QStringLiteral(", "))));
    QVERIFY(sequences.contains(QDir(repoDir()).filePath(QStringLiteral("default.seq.json"))));
    QVERIFY(!repoFiles(QStringLiteral("*.templ.json")).isEmpty());
}

void TestPromptData::repoSequenceParses_data()
{
    QTest::addColumn<QString>("path");
    const QDir root(repoDir());
    for (const QString& p : repoFiles(QStringLiteral("*.seq.json")))
        QTest::newRow(root.relativeFilePath(p).toUtf8().constData()) << p;
}

void TestPromptData::repoSequenceParses()
{
    QFETCH(QString, path);
    QString error;
    const QJsonObject obj = readJsonObject(path, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    const PromptSequence seq = PromptSequence::fromJson(obj);
    QVERIFY(!seq.name().trimmed().isEmpty());
    QVERIFY(!seq.steps().isEmpty());

    const QSet<QString> known = registeredNames();
    QStringList unknown;
    for (int i = 0; i < seq.steps().size(); ++i) {
        const PromptStep& step = seq.steps().at(i);
        QVERIFY2(!step.tmpl().trimmed().isEmpty(), qPrintable(QStringLiteral("step %1 has no template").arg(i + 1)));
        for (const QString& name : step.allowedCommands()) {
            if (!known.contains(name))
                unknown << QStringLiteral("step %1: %2").arg(i + 1).arg(name);
        }
    }
    QVERIFY2(unknown.isEmpty(), qPrintable(QStringLiteral("not registered commands: ") + unknown.join(QStringLiteral("; "))));
}

void TestPromptData::repoTemplateIsConsistent_data()
{
    QTest::addColumn<QString>("path");
    const QDir root(repoDir());
    for (const QString& p : repoFiles(QStringLiteral("*.templ.json")))
        QTest::newRow(root.relativeFilePath(p).toUtf8().constData()) << p;
}

void TestPromptData::repoTemplateIsConsistent()
{
    QFETCH(QString, path);
    QString error;
    const SolutionTemplate t = SolutionTemplate::fromJson(readJsonObject(path, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(!t.name().trimmed().isEmpty());
    QVERIFY(!t.sequences().isEmpty());

    const QDir dir = QFileInfo(path).dir();
    for (const QString& seq : t.sequences())
        QVERIFY2(QFileInfo::exists(dir.filePath(seq)), qPrintable(QStringLiteral("missing sequence file ") + seq));
    QVERIFY2(t.mainSequencePath().isEmpty() || t.sequences().contains(t.mainSequencePath()),
             qPrintable(t.mainSequencePath()));

    const QJsonObject targets = t.getSequenceTargets();
    for (auto it = targets.constBegin(); it != targets.constEnd(); ++it) {
        QVERIFY2(t.sequences().contains(it.key()), qPrintable(it.key()));
        for (const QJsonValue& target : it.value().toArray())
            QVERIFY2(t.sequences().contains(target.toString()), qPrintable(target.toString()));
    }
}

QTEST_MAIN(TestPromptData)
#include "tst_promptdata.moc"
