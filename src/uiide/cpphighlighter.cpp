#include "cpphighlighter.h"
#include <QColor>

CppHighlighter::CppHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {
    keywordFormat.setForeground(QColor(86,0,179));
    keywordFormat.setFontWeight(QFont::Bold);

    const QStringList keywords = {
        "alignas","alignof","and","and_eq","asm","atomic_cancel","atomic_commit","atomic_noexcept",
        "auto","bitand","bitor","bool","break","case","catch","char","char8_t","char16_t","char32_t",
        "class","compl","const","consteval","constexpr","constinit","const_cast","continue","co_await","co_return","co_yield",
        "decltype","default","delete","do","double","dynamic_cast","else","enum","explicit","export","extern",
        "false","float","for","friend","goto","if","inline","int","long","mutable","namespace","new","noexcept",
        "not","not_eq","nullptr","operator","or","or_eq","private","protected","public","register","reinterpret_cast",
        "requires","return","short","signed","sizeof","static","static_assert","static_cast","struct","switch","synchronized",
        "template","this","thread_local","throw","true","try","typedef","typeid","typename","union","unsigned","using",
        "virtual","void","volatile","wchar_t","while","xor","xor_eq"
    };
    for (const QString &kw : keywords) {
        Rule r;
        r.pattern = QRegularExpression(QString("\\b%1\\b").arg(kw));
        r.format = keywordFormat;
        rules.push_back(r);
    }

    classFormat.setForeground(QColor(0,102,153));
    rules.push_back({QRegularExpression("\\b[A-Z]\\w+\\b"), classFormat});

    functionFormat.setForeground(QColor(0,102,0));
    rules.push_back({QRegularExpression("\\b[a-zA-Z_]\\w*(?=\\s*\\()"), functionFormat});

    numberFormat.setForeground(QColor(153,0,0));
    rules.push_back({QRegularExpression("\\b(0x[0-9A-Fa-f]+|\\d+\\.?\\d*|\\.\\d+)\\b"), numberFormat});

    stringFormat.setForeground(QColor(196,26,22));
    rules.push_back({QRegularExpression(R"("([^"\\]|\\.)*")"), stringFormat});
    rules.push_back({QRegularExpression(R"('([^'\\]|\\.)*')"), stringFormat});

    preprocFormat.setForeground(QColor(128, 0, 128));
    rules.push_back({QRegularExpression(R"(^\s*#\s*\w+.*)"), preprocFormat});

    singleLineCommentFormat.setForeground(QColor(120,120,120));
    rules.push_back({QRegularExpression(R"(//[^\n]*)"), singleLineCommentFormat});

    multiLineCommentFormat.setForeground(QColor(120,120,120));
    commentStart = QRegularExpression(R"(/\*)");
    commentEnd   = QRegularExpression(R"(\*/)");
}

void CppHighlighter::highlightBlock(const QString &text) {
    for (const auto &r : rules) {
        auto it = r.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), r.format);
        }
    }

    // Multiline comments
    setCurrentBlockState(0);
    int start = 0;
    if (previousBlockState() != 1)
        start = text.indexOf(commentStart);
    else
        start = 0;

    while (start >= 0) {
        auto m = commentEnd.match(text, start);
        int end = m.hasMatch() ? m.capturedStart() : text.length();
        int len = (m.hasMatch() ? m.capturedEnd() : text.length()) - start;
        setFormat(start, len, multiLineCommentFormat);
        if (!m.hasMatch()) {
            setCurrentBlockState(1);
            break;
        }
        start = text.indexOf(commentStart, end);
    }
}
