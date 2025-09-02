#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QDir>
#include "SymbolModel.h"

class CodePatcher : public QObject{
    Q_OBJECT
public:
    explicit CodePatcher(QObject* parent=nullptr);

    bool renameMethod(const NodeData &method, const QString &newName);
    bool addMethodStub(const NodeData &clazz, const QString &retType, const QString &name, const QVector<ParamInfo>& params);

    QString stashFile(const QString &filePath);
    bool restoreFromStash(const QString &filePath);

private:
    bool replaceInFile(const QString &filePath, const QRegExp &re, const QString &replacement);
    QString stashDirForFile(const QString &filePath) const;
};
