// PythonHighlighter.cpp
#include "PythonHighlighter.h"
#include <QColor>

PythonHighlighter::PythonHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // ── форматирование ключевых слов ─────────────────────
    keywordFormat.setForeground(QColor(86,0,179));
    keywordFormat.setFontWeight(QFont::Bold);

    static const QStringList keywords = {
        "False", "class", "finally", "is", "return",
        "None", "continue", "for", "lambda", "try",
        "True", "def", "from", "nonlocal", "while",
        "and", "del", "global", "not", "with",
        "as", "elif", "if", "or", "yield",
        "assert", "else", "import", "pass",
        "break", "except", "in", "raise",
        "async", "await", "global", "nonlocal", "type"
    };
    for (const QString &kw : keywords) {
        rules.append({ QRegularExpression(QString("\\b%1\\b").arg(kw)), keywordFormat });
    }

    // ── форматирование строк ───────────────────────────────
    stringFormat.setForeground(QColor(196,26,22));
    // двойные кавычки
    rules.append({ QRegularExpression(R"("([^"\\]|\\.)*")"), stringFormat });
    // одинарные кавычки
    rules.append({ QRegularExpression(R"('([^'\\]|\\.)*')"), stringFormat });

    // ── форматирование чисел ───────────────────────────────
    numberFormat.setForeground(QColor(153,0,0));
    rules.append({ QRegularExpression(R"(\b\d+(\.\d+)?\b)"), numberFormat });

    // ── форматирование комментариев ────────────────────────
    commentFormat.setForeground(QColor(120,120,120));
    // однострочный
    rules.append({ QRegularExpression(R"(#.*)"), commentFormat });

    // ── многострочные строки ('''  и  """) ───────────────
    tripleQuotePattern = QRegularExpression(R"(('''|"""[\s\S]*?(?:'''|""")))");
    // комментарий (не нужен для Python, но оставим для совместимости)
    commentStart = QRegularExpression(R"(#)");
}


void PythonHighlighter::highlightBlock(const QString &text)
{
    // 1️⃣ Поиск обычных шаблонов
    for (const auto &r : rules) {
        QRegularExpressionMatchIterator i = r.pattern.globalMatch(text);
        while (i.hasNext()) {
            QRegularExpressionMatch m = i.next();
            setFormat(m.capturedStart(), m.capturedLength(), r.format);
        }
    }

    // 2️⃣ Многострочные строки ('''  и  """) – они могут пересекаться со строками
    int startIndex = 0;
    if (previousBlockState() == 1) {
        startIndex = 0;   // продолжаем с начала блока
    } else {
        startIndex = text.indexOf(tripleQuotePattern);
    }

    while (startIndex >= 0) {
        QRegularExpressionMatch m = tripleQuotePattern.match(text, startIndex);
        int endIndex = m.capturedStart();   // начало закрывающего тега
        int length   = m.capturedLength();
        if (endIndex < 0) {
            // открытая строка до конца блока – продолжаем в следующем блоке
            setFormat(startIndex, text.length() - startIndex, stringFormat);
            setCurrentBlockState(1);
            break;
        } else {
            // закрытая строка внутри блока – окрашиваем полностью
            setFormat(startIndex, endIndex + length - startIndex, stringFormat);
        }
        startIndex = text.indexOf(tripleQuotePattern, endIndex + length);
    }
}
