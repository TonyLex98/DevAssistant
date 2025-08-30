// DartHighlighter.h
#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QVector>

class DartHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit DartHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule { QRegularExpression pattern; QTextCharFormat format; };
    QVector<Rule> rules;

    QTextCharFormat keywordFormat;
    QTextCharFormat classFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat numberFormat;

    QRegularExpression multilineCommentStart; // /* …
    QRegularExpression multilineCommentEnd;   // … */
    QRegularExpression commentStart;          // //
};
