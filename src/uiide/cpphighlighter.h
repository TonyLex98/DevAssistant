#pragma once
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

class CppHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit CppHighlighter(QTextDocument *parent = nullptr);


protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule { QRegularExpression pattern; QTextCharFormat format; };
    QVector<Rule> rules;
    QTextCharFormat keywordFormat, classFormat, stringFormat, singleLineCommentFormat, multiLineCommentFormat, numberFormat, functionFormat, preprocFormat;

    QRegularExpression commentStart, commentEnd;
};
