// PythonHighlighter.h
#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QVector>

class PythonHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit PythonHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule { QRegularExpression pattern; QTextCharFormat format; };
    QVector<Rule> rules;

    // Formats
    QTextCharFormat keywordFormat;
    QTextCharFormat classFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat numberFormat;

    // Patterns for multi-line strings and comments
    QRegularExpression tripleQuotePattern;   // ''' or """
    QRegularExpression commentStart;          // # (single-line)
};
