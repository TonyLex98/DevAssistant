#include "CreateFileScenario.h"
QString CreateFileScenario::parse(const QString& instruction,const QString& projectRoot,QVector<Change>& out){
    QRegularExpression header(R"(CREATE_FILE\s+path=(\S+))", QRegularExpression::CaseInsensitiveOption);
    auto m = header.match(instruction);
    if (!m.hasMatch()) return QString();
    QString rel = m.captured(1);
    int s = instruction.indexOf("\nCONTENT:\n");
    int e = instruction.lastIndexOf("\nEND");
    if (s<0 || e<0 || e<=s) return "Missing CONTENT/END section.";
    QString content = instruction.mid(s+10, e-(s+10));
    QString abs = QDir::isAbsolutePath(rel)? rel : QDir(projectRoot).filePath(rel);
    out.push_back(Change::createFile(abs, content.toUtf8()));
    return QString();
}
