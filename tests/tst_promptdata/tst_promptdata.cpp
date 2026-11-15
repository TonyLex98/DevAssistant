#include <QtTest>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

#include "core/workflow/PromptSequence.h"
#include "core/workflow/PromptStep.h"
#include "core/workflow/SolutionTemplate.h"

namespace {

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

QTEST_MAIN(TestPromptData)
#include "tst_promptdata.moc"
