#include "DeleteFileScenario.h"
QString DeleteFileScenario::parse(const QString& instruction,const QString& projectRoot,QVector<Change>& out){
    QRegularExpression header(R"(DELETE_FILE\s+path=(\S+))", QRegularExpression::CaseInsensitiveOption);
    auto m = header.match(instruction);
    if (!m.hasMatch()) return QString();
    QString rel = m.captured(1);
    QString abs = QDir::isAbsolutePath(rel)? rel : QDir(projectRoot).filePath(rel);
    out.push_back(Change::deleteFile(abs));
    return QString();
}
