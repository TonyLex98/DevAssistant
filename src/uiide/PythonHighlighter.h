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

    // Форматы
    QTextCharFormat keywordFormat;
    QTextCharFormat classFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat numberFormat;

    // Regex для многострочных строк и комментариев
    QRegularExpression tripleQuotePattern;   // '''  или  """  (три одиночные/двойные кавычки)
    QRegularExpression commentStart;          // #  (однострочный)
};
