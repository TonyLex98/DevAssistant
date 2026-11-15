#include <QtTest>
#include <QDir>
#include <QTemporaryDir>

#include "core/code/CodePatcher.h"
#include "core/code/ProjectIndexer.h"
#include "TestFiles.h"

namespace {

const char kCpp[] =
        "#include <cstdio>\n"                         //  1
        "\n"                                          //  2
        "class Foo {\n"                               //  3
        "public:\n"                                   //  4
        "    void bar();\n"                           //  5
        "    int get() const { return count; }\n"     //  6
        "    int count = 0;\n"                        //  7
        "};\n"                                        //  8
        "\n"                                          //  9
        "struct Point {\n"                            // 10
        "    int x;\n"                                // 11
        "    int y;\n"                                // 12
        "};\n"                                        // 13
        "\n"                                          // 14
        "static int helper(int v)\n"                  // 15
        "{\n"                                         // 16
        "    return v + 1;\n"                         // 17
        "}\n"                                         // 18
        "\n"                                          // 19
        "void Foo::bar()\n"                           // 20
        "{\n"                                         // 21
        "    count = helper(count);\n"                // 22
        "    std::puts(\"bar\");\n"                   // 23
        "}\n";                                        // 24

const char kPython[] =
        "import os\n"                                 //  1
        "\n"                                          //  2
        "\n"                                          //  3
        "class Greeter:\n"                            //  4
        "    def __init__(self, name):\n"             //  5
        "        self.name = name\n"                  //  6
        "\n"                                          //  7
        "    def greet(self):\n"                      //  8
        "        return format_name(self.name)\n"     //  9
        "\n"                                          // 10
        "\n"                                          // 11
        "def format_name(name):\n"                    // 12
        "    return name.title()\n";                  // 13

const char kDart[] =
        "class Counter {\n"                           //  1
        "  int value = 0;\n"                          //  2
        "\n"                                          //  3
        "  void increment() {\n"                      //  4
        "    value++;\n"                              //  5
        "  }\n"                                       //  6
        "}\n"                                         //  7
        "\n"                                          //  8
        "int add(int a, int b) {\n"                   //  9
        "  return a + b;\n"                           // 10
        "}\n";                                        // 11

// The first node of `kind` named `name`; an empty node (no sid) if there is none.
NodeData findNode(const SymbolGraph& g, NodeKind kind, const QString& name)
{
    for (const NodeData& n : g.nodes) {
        if (n.kind == kind && n.canonicalName == name)
            return n;
    }
    return NodeData();
}

int countNodes(const SymbolGraph& g, NodeKind kind, const QString& name)
{
    int count = 0;
    for (const NodeData& n : g.nodes) {
        if (n.kind == kind && n.canonicalName == name)
            ++count;
    }
    return count;
}

bool hasCall(const SymbolGraph& g, const QString& fromSid, const QString& toSid)
{
    for (const EdgeData& e : g.edges) {
        if (e.kind == EdgeKind::Calls && e.fromSid == fromSid && e.toSid == toSid)
            return true;
    }
    return false;
}

QString parentName(const SymbolGraph& g, const NodeData& n)
{
    return g.nodes.value(n.parentSid).displayName;
}

SymbolGraph indexOne(const QString& path)
{
    ProjectIndexer indexer;
    SymbolGraph g;
    indexer.indexFile(path, g);
    indexer.resolveCalls(g);
    return g;
}

} // namespace

class TestCodeViewer : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void languageForSuffix();
    void cppClassesMethodsAndFields();
    void cppCallLinkedToDefinition();
    void pythonClassMethodAndCall();
    void dartClassAndFunction();
    void buildGraphLinksCallsAcrossFiles();

    void renameMethod();
    void renameMethodKeepsLineEndings();
    void restoreFromStashUsesThisFilesBackup();
    void restoreFromStashUsesNewestBackup();

private:
    QString writeSample(const QString& name, const QByteArray& content);

    QTemporaryDir m_tmp;
};

void TestCodeViewer::initTestCase()
{
    QVERIFY(m_tmp.isValid());
}

QString TestCodeViewer::writeSample(const QString& name, const QByteArray& content)
{
    const QString path = m_tmp.filePath(name);
    return TestFiles::write(path, content) ? path : QString();
}

void TestCodeViewer::languageForSuffix()
{
    QVERIFY(ProjectIndexer::languageForSuffix(QStringLiteral(".cpp")) == tree_sitter_cpp());
    QVERIFY(ProjectIndexer::languageForSuffix(QStringLiteral(".H")) == tree_sitter_cpp());
    QVERIFY(ProjectIndexer::languageForSuffix(QStringLiteral(".py")) == tree_sitter_python());
    QVERIFY(ProjectIndexer::languageForSuffix(QStringLiteral(".dart")) == tree_sitter_dart());
    QVERIFY(ProjectIndexer::languageForSuffix(QStringLiteral(".txt")) == nullptr);
    QVERIFY(ProjectIndexer::languageForSuffix(QString()) == nullptr);
}

void TestCodeViewer::cppClassesMethodsAndFields()
{
    const QString path = writeSample(QStringLiteral("cpp/sample.cpp"), kCpp);
    QVERIFY(!path.isEmpty());
    const SymbolGraph g = indexOne(path);

    QVERIFY(!findNode(g, NodeKind::Language, QStringLiteral("sample.cpp")).sid.id.isEmpty());

    const NodeData foo = findNode(g, NodeKind::Class, QStringLiteral("Foo"));
    QVERIFY2(!foo.sid.id.isEmpty(), "class Foo not found");
    QCOMPARE(foo.loc.file, path);
    QCOMPARE(foo.loc.line, 3);
    QCOMPARE(countNodes(g, NodeKind::Class, QStringLiteral("Foo")), 1);

    const NodeData bar = findNode(g, NodeKind::Method, QStringLiteral("Foo::bar"));
    QVERIFY2(!bar.sid.id.isEmpty(), "method Foo::bar not found");
    QCOMPARE(bar.parentSid, foo.sid.id);
    QCOMPARE(bar.loc.line, 20);

    const NodeData get = findNode(g, NodeKind::Method, QStringLiteral("get"));
    QVERIFY2(!get.sid.id.isEmpty(), "inline method get not found");
    QCOMPARE(get.parentSid, foo.sid.id);

    const NodeData helper = findNode(g, NodeKind::Method, QStringLiteral("helper"));
    QVERIFY2(!helper.sid.id.isEmpty(), "free function helper not found");
    QCOMPARE(parentName(g, helper), QStringLiteral("__Global::sample.cpp"));
    QCOMPARE(helper.loc.line, 15);

    const NodeData count = findNode(g, NodeKind::Field, QStringLiteral("count"));
    QVERIFY2(!count.sid.id.isEmpty(), "data member count not found");
    QCOMPARE(count.parentSid, foo.sid.id);
    QCOMPARE(count.loc.line, 7);
    // "void bar();" declares a method, not a data member.
    QVERIFY(findNode(g, NodeKind::Field, QStringLiteral("bar")).sid.id.isEmpty());

    const NodeData point = findNode(g, NodeKind::Class, QStringLiteral("Point"));
    QVERIFY2(!point.sid.id.isEmpty(), "struct Point not found");
    QCOMPARE(findNode(g, NodeKind::Field, QStringLiteral("x")).parentSid, point.sid.id);
    QCOMPARE(findNode(g, NodeKind::Field, QStringLiteral("y")).parentSid, point.sid.id);
}

void TestCodeViewer::cppCallLinkedToDefinition()
{
    const QString path = writeSample(QStringLiteral("cpp-calls/sample.cpp"), kCpp);
    QVERIFY(!path.isEmpty());
    const SymbolGraph g = indexOne(path);

    const NodeData bar = findNode(g, NodeKind::Method, QStringLiteral("Foo::bar"));
    const NodeData helper = findNode(g, NodeKind::Method, QStringLiteral("helper"));
    QVERIFY(!bar.sid.id.isEmpty() && !helper.sid.id.isEmpty());
    QVERIFY2(hasCall(g, bar.sid.id, helper.sid.id), "the call Foo::bar -> helper is not linked to helper's definition");
    QVERIFY2(findNode(g, NodeKind::Function, QStringLiteral("helper")).sid.id.isEmpty(),
             "helper also shows up as an external function");

    // A function defined nowhere in the project goes to __External.
    const NodeData puts = findNode(g, NodeKind::Function, QStringLiteral("puts"));
    QVERIFY2(!puts.sid.id.isEmpty(), "external call std::puts not found");
    QCOMPARE(parentName(g, puts), QStringLiteral("__External"));
    QVERIFY(hasCall(g, bar.sid.id, puts.sid.id));
}

void TestCodeViewer::pythonClassMethodAndCall()
{
    const QString path = writeSample(QStringLiteral("py/sample.py"), kPython);
    QVERIFY(!path.isEmpty());
    const SymbolGraph g = indexOne(path);

    const NodeData greeter = findNode(g, NodeKind::Class, QStringLiteral("Greeter"));
    QVERIFY2(!greeter.sid.id.isEmpty(), "class Greeter not found");
    QCOMPARE(greeter.loc.line, 4);

    const NodeData init = findNode(g, NodeKind::Method, QStringLiteral("__init__"));
    QCOMPARE(init.parentSid, greeter.sid.id);
    const NodeData greet = findNode(g, NodeKind::Method, QStringLiteral("greet"));
    QVERIFY2(!greet.sid.id.isEmpty(), "method greet not found");
    QCOMPARE(greet.parentSid, greeter.sid.id);
    QCOMPARE(greet.loc.line, 8);

    const NodeData format = findNode(g, NodeKind::Method, QStringLiteral("format_name"));
    QVERIFY2(!format.sid.id.isEmpty(), "function format_name not found");
    QCOMPARE(parentName(g, format), QStringLiteral("__Global::sample.py"));

    QVERIFY2(hasCall(g, greet.sid.id, format.sid.id), "the call greet -> format_name is not linked");
    const NodeData title = findNode(g, NodeKind::Function, QStringLiteral("title"));
    QCOMPARE(parentName(g, title), QStringLiteral("__External"));
    QVERIFY(hasCall(g, format.sid.id, title.sid.id));
}

void TestCodeViewer::dartClassAndFunction()
{
    const QString path = writeSample(QStringLiteral("dart/sample.dart"), kDart);
    QVERIFY(!path.isEmpty());
    const SymbolGraph g = indexOne(path);

    const NodeData counter = findNode(g, NodeKind::Class, QStringLiteral("Counter"));
    QVERIFY2(!counter.sid.id.isEmpty(), "class Counter not found");
    QCOMPARE(counter.loc.line, 1);

    const NodeData increment = findNode(g, NodeKind::Method, QStringLiteral("increment"));
    QVERIFY2(!increment.sid.id.isEmpty(), "method increment not found");
    QCOMPARE(increment.parentSid, counter.sid.id);

    const NodeData add = findNode(g, NodeKind::Method, QStringLiteral("add"));
    QVERIFY2(!add.sid.id.isEmpty(), "function add not found");
    QCOMPARE(parentName(g, add), QStringLiteral("__Global::sample.dart"));
    QCOMPARE(add.loc.line, 9);

    // Calls are not indexed for Dart.
    QVERIFY(g.edges.isEmpty());
}

void TestCodeViewer::buildGraphLinksCallsAcrossFiles()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(TestFiles::write(root.filePath(QStringLiteral("a.cpp")),
                             "int compute(int x);\n\nint run()\n{\n    return compute(2);\n}\n"));
    QVERIFY(TestFiles::write(root.filePath(QStringLiteral("lib/b.cpp")),
                             "int compute(int x)\n{\n    return x * 2;\n}\n"));
    QVERIFY(TestFiles::write(root.filePath(QStringLiteral("build/c.cpp")), "void skipped() {}\n"));
    QVERIFY(TestFiles::write(root.filePath(QStringLiteral("notes.txt")), "int ignored() { return 0; }\n"));

    ProjectIndexer indexer;
    indexer.setRoot(root.path());
    const SymbolGraph g = indexer.buildGraphSync();

    const NodeData run = findNode(g, NodeKind::Method, QStringLiteral("run"));
    const NodeData compute = findNode(g, NodeKind::Method, QStringLiteral("compute"));
    QVERIFY2(!run.sid.id.isEmpty() && !compute.sid.id.isEmpty(), "run or compute not found");
    QCOMPARE(compute.loc.file, root.filePath(QStringLiteral("lib/b.cpp")));
    QVERIFY2(hasCall(g, run.sid.id, compute.sid.id), "the call run -> compute in another file is not linked");
    QVERIFY(findNode(g, NodeKind::Function, QStringLiteral("compute")).sid.id.isEmpty());

    QVERIFY2(findNode(g, NodeKind::Method, QStringLiteral("skipped")).sid.id.isEmpty(), "build/ was indexed");
    QVERIFY2(findNode(g, NodeKind::Method, QStringLiteral("ignored")).sid.id.isEmpty(), "a .txt file was indexed");
}

void TestCodeViewer::renameMethod()
{
    const QByteArray original =
            "class Foo {\n"
            "public:\n"
            "    void bar();\n"
            "    void foobar();\n"
            "};\n"
            "\n"
            "void Foo::bar() {}\n"
            "void use(Foo &f) { f.bar(); f.foobar(); }\n";
    const QString path = writeSample(QStringLiteral("rename/foo.cpp"), original);
    QVERIFY(!path.isEmpty());

    NodeData method;
    method.kind = NodeKind::Method;
    method.canonicalName = QStringLiteral("bar");
    method.loc.file = path;

    CodePatcher patcher;
    QVERIFY(patcher.renameMethod(method, QStringLiteral("baz")));

    QString text = QString::fromUtf8(TestFiles::read(path));
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));  // line endings: see renameMethodKeepsLineEndings
    QCOMPARE(text, QStringLiteral("class Foo {\n"
                                  "public:\n"
                                  "    void baz();\n"
                                  "    void foobar();\n"
                                  "};\n"
                                  "\n"
                                  "void Foo::baz() {}\n"
                                  "void use(Foo &f) { f.baz(); f.foobar(); }\n"));

    // The original is kept in the stash folder next to the file.
    const QDir stash(QFileInfo(path).absolutePath() + QStringLiteral("/.graph_stash"));
    const QStringList backups = stash.entryList(QStringList{QStringLiteral("foo.cpp_*.bak")}, QDir::Files);
    QCOMPARE(backups.size(), 1);
    QCOMPARE(TestFiles::read(stash.filePath(backups.first())), original);

    NodeData missing = method;
    missing.canonicalName = QStringLiteral("notThere");
    QVERIFY(!patcher.renameMethod(missing, QStringLiteral("x")));
    NodeData noFile;
    noFile.canonicalName = QStringLiteral("baz");
    QVERIFY(!patcher.renameMethod(noFile, QStringLiteral("x")));
}

void TestCodeViewer::renameMethodKeepsLineEndings()
{
    const QString path = writeSample(QStringLiteral("eol/foo.cpp"), "void bar();\nvoid bar() {}\n");
    QVERIFY(!path.isEmpty());
    NodeData method;
    method.canonicalName = QStringLiteral("bar");
    method.loc.file = path;

    CodePatcher patcher;
    QVERIFY(patcher.renameMethod(method, QStringLiteral("baz")));
    QCOMPARE(TestFiles::read(path), QByteArray("void baz();\nvoid baz() {}\n"));
}

void TestCodeViewer::restoreFromStashUsesThisFilesBackup()
{
    const QString a = writeSample(QStringLiteral("restore/a.cpp"), "A-original\n");
    const QString b = writeSample(QStringLiteral("restore/b.cpp"), "B-original\n");
    const QString c = writeSample(QStringLiteral("restore/c.cpp"), "C\n");
    QVERIFY(!a.isEmpty() && !b.isEmpty() && !c.isEmpty());

    CodePatcher patcher;
    QVERIFY(!patcher.stashFile(a).isEmpty());
    QTest::qSleep(100);
    QVERIFY(!patcher.stashFile(b).isEmpty());  // now the newest backup in the folder is b's

    QVERIFY(TestFiles::write(a, "A-changed\n"));
    QVERIFY(TestFiles::write(b, "B-changed\n"));

    QVERIFY(patcher.restoreFromStash(a));
    QCOMPARE(TestFiles::read(a), QByteArray("A-original\n"));
    QCOMPARE(TestFiles::read(b), QByteArray("B-changed\n"));

    QVERIFY(patcher.restoreFromStash(b));
    QCOMPARE(TestFiles::read(b), QByteArray("B-original\n"));

    QVERIFY2(!patcher.restoreFromStash(c), "c.cpp has no backup of its own");
    QCOMPARE(TestFiles::read(c), QByteArray("C\n"));
}

void TestCodeViewer::restoreFromStashUsesNewestBackup()
{
    const QString path = writeSample(QStringLiteral("newest/f.cpp"), "v1\n");
    QVERIFY(!path.isEmpty());

    CodePatcher patcher;
    QVERIFY(!patcher.stashFile(path).isEmpty());
    QTest::qSleep(100);
    QVERIFY(TestFiles::write(path, "v2\n"));
    QVERIFY(!patcher.stashFile(path).isEmpty());
    QVERIFY(TestFiles::write(path, "v3\n"));

    QVERIFY(patcher.restoreFromStash(path));
    QCOMPARE(TestFiles::read(path), QByteArray("v2\n"));
}

QTEST_MAIN(TestCodeViewer)
#include "tst_codeviewer.moc"
