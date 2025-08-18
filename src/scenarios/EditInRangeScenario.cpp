#include "EditInRangeScenario.h"
QString EditInRangeScenario::parse(const QString& instruction,const QString& projectRoot,QVector<Change>& out){
    QRegularExpression rx(R"(EDIT_IN_RANGE\s+path=(\S+)\s+from=(\d+)\s+to=(\d+))", QRegularExpression::CaseInsensitiveOption);
    auto m = rx.match(instruction);
    if (!m.hasMatch()) return QString();
    QString rel = m.captured(1);
    int l1 = m.captured(2).toInt();
    int l2 = m.captured(3).toInt();
    if (l1<=0 || l2<l1) return "Invalid line range.";
    int s = instruction.indexOf("\nCONTENT:\n");
    int e = instruction.lastIndexOf("\nEND");
    if (s<0 || e<0 || e<=s) return "Missing CONTENT/END section.";
    QString replacement = instruction.mid(s+10, e-(s+10));
    QString abs = QDir::isAbsolutePath(rel)? rel : QDir(projectRoot).filePath(rel);
    QFile f(abs); if (!f.open(QIODevice::ReadOnly)) return "Cannot open target file for reading.";
    QString content = QString::fromUtf8(f.readAll()); f.close();
    QStringList lines = content.split('\n');
    if (l2>lines.size()) return "Target range exceeds file length.";
    QStringList repl = replacement.split('\n');
    QStringList outLines; outLines.reserve(lines.size()-(l2-l1+1)+repl.size());
    for (int i=0;i<l1-1;++i) outLines<<lines[i];
    for (const QString& r : repl) outLines<<r;
    for (int i=l2;i<lines.size();++i) outLines<<lines[i];
    QByteArray newContent = outLines.join("\n").toUtf8();
    out.push_back(Change::replaceInFile(abs, newContent));
    return QString();
}
