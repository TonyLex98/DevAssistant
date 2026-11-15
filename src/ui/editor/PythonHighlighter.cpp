// PythonHighlighter.cpp
#include "ui/editor/PythonHighlighter.h"
#include <QColor>

PythonHighlighter::PythonHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // ── keywords ─────────────────────────────────────────
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

    // ── strings ────────────────────────────────────────────
    stringFormat.setForeground(QColor(196,26,22));
    // double quotes
    rules.append({ QRegularExpression(R"("([^"\\]|\\.)*")"), stringFormat });
    // single quotes
    rules.append({ QRegularExpression(R"('([^'\\]|\\.)*')"), stringFormat });

    // ── numbers ────────────────────────────────────────────
    numberFormat.setForeground(QColor(153,0,0));
    rules.append({ QRegularExpression(R"(\b\d+(\.\d+)?\b)"), numberFormat });

    // ── comments ───────────────────────────────────────────
    commentFormat.setForeground(QColor(120,120,120));
    rules.append({ QRegularExpression(R"(#.*)"), commentFormat });

    // ── triple-quoted strings (''' and """) ──────────────
    tripleQuotePattern = QRegularExpression(R"(('''|"""[\s\S]*?(?:'''|""")))");
    // comment start (unused for Python, kept for symmetry with other highlighters)
    commentStart = QRegularExpression(R"(#)");
}


void PythonHighlighter::highlightBlock(const QString &text)
{
    // Single-line rules
    for (const auto &r : rules) {
        QRegularExpressionMatchIterator i = r.pattern.globalMatch(text);
        while (i.hasNext()) {
            QRegularExpressionMatch m = i.next();
            setFormat(m.capturedStart(), m.capturedLength(), r.format);
        }
    }

    // Triple-quoted strings (''' and """); may span several blocks
    int startIndex = 0;
    if (previousBlockState() == 1) {
        startIndex = 0;   // continue from the start of the block
    } else {
        startIndex = text.indexOf(tripleQuotePattern);
    }

    while (startIndex >= 0) {
        QRegularExpressionMatch m = tripleQuotePattern.match(text, startIndex);
        int endIndex = m.capturedStart();
        int length   = m.capturedLength();
        if (endIndex < 0) {
            // unterminated: highlight to the end and continue in the next block
            setFormat(startIndex, text.length() - startIndex, stringFormat);
            setCurrentBlockState(1);
            break;
        } else {
            // terminated inside this block
            setFormat(startIndex, endIndex + length - startIndex, stringFormat);
        }
        startIndex = text.indexOf(tripleQuotePattern, endIndex + length);
    }
}
