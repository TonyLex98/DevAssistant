// DartHighlighter.cpp
#include "ui/editor/DartHighlighter.h"
#include <QColor>


DartHighlighter::DartHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // ── Dart keywords ──────────────────────────────────────
    keywordFormat.setForeground(QColor(86,0,179));
    keywordFormat.setFontWeight(QFont::Bold);

    static const QStringList keywords = {
        "abstract", "continue", "for", "in", "new", "return", "super", "typedef",
        "as", "default", "if", "is", "switch", "this", "var", "void", "while",
        "break", "do", "else", "operator", "throw", "class", "enum", "library",
        "implements", "operator", "true", "case", "extends", "mixin", "var",
        "false", "factory", "import", "new", "void", "with", "final", "static",
        "async", "await", "dynamic", "final", "var", "null"
    };
    for (const QString &kw : keywords) {
        rules.append({ QRegularExpression(QString("\\b%1\\b").arg(kw)), keywordFormat });
    }

    // ── strings ────────────────────────────────────────────
    stringFormat.setForeground(QColor(196,26,22));
    rules.append({ QRegularExpression(R"("([^"\\]|\\.)*")"), stringFormat });
    rules.append({ QRegularExpression(R"('([^'\\]|\\.)*')"), stringFormat });

    // ── numbers ───────────────────────────────────────────
    numberFormat.setForeground(QColor(153,0,0));
    rules.append({ QRegularExpression(R"(\b\d+(\.\d+)?\b)"), numberFormat });

    // ── single-line comments // … ─────────────────────────
    commentFormat.setForeground(QColor(120,120,120));
    rules.append({ QRegularExpression(R"(//.*)"), commentFormat });

    // ── multi-line comments /* … */ ───────────────────────
    multilineCommentStart = QRegularExpression(R"(/\*)");
    multilineCommentEnd   = QRegularExpression(R"(\*/)");
    commentStart          = QRegularExpression(R"(//)");
}

void DartHighlighter::highlightBlock(const QString &text)
{
    // Single-line rules (keywords, strings, numbers, // comments)
    for (const auto &r : rules) {
        QRegularExpressionMatchIterator i = r.pattern.globalMatch(text);
        while (i.hasNext()) {
            QRegularExpressionMatch m = i.next();
            setFormat(m.capturedStart(), m.capturedLength(), r.format);
        }
    }

    // Multi-line comments
    int startIndex = 0;
    if (previousBlockState() == 1) {
        startIndex = 0;          // comment continues from the previous block
    } else {
        startIndex = text.indexOf(multilineCommentStart);
    }

    while (startIndex >= 0) {
        int endIndex = text.indexOf(multilineCommentEnd, startIndex + 2);
        if (endIndex < 0) {      // not closed in this block
            setFormat(startIndex, text.length() - startIndex, commentFormat);
            setCurrentBlockState(1);   // comment still open
            break;
        } else {                 // closed
            setFormat(startIndex, endIndex - startIndex + 2, commentFormat);
            startIndex = text.indexOf(multilineCommentStart, endIndex + 2);
        }
    }
}
