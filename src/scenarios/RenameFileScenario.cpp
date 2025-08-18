#include "RenameFileScenario.h"
QString RenameFileScenario::parse(const QString& instruction,const QString& projectRoot,QVector<Change>& out){
    QRegularExpression header(R"(RENAME_FILE\s+from=(\S+)\s+to=(\S+))", QRegularExpression::CaseInsensitiveOption);
    auto m = header.match(instruction);
    if (!m.hasMatch()) return QString();
    QString fromRel = m.captured(1);
    QString toRel = m.captured(2);
    QString fromAbs = QDir::isAbsolutePath(fromRel)? fromRel : QDir(projectRoot).filePath(fromRel);
    QString toAbs   = QDir::isAbsolutePath(toRel)? toRel : QDir(projectRoot).filePath(toRel);
    out.push_back(Change::renameFile(fromAbs, toAbs));
    return QString();
}
