#include "ResultDisplayWidget.h"
#include <QTextCursor>
#include <QTextBlock>

ResultDisplayWidget::ResultDisplayWidget(QWidget *parent)
    : QWidget(parent)
{
    // Create the text edit that will display the results
    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    
    // Set styling for the text edit
    m_textEdit->setStyleSheet(
        "QTextEdit {"
        "  background-color: #ffffff;"
        "  border: 1px solid #cccccc;"
        "  border-radius: 5px;"
        "  padding: 10px;"
        "  font-family: Arial, sans-serif;"
        "  font-size: 14px;"
        "  line-height: 1.4;"
        "}"
    );

    // Create layout and add the text edit
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_textEdit);
    setLayout(layout);
}

void ResultDisplayWidget::appendResult(const QString &title, const QString &content)
{
    QString htmlContent = content;
    
    // Convert markdown to HTML
    convertMarkdownToHtml(htmlContent);
    
    // Move cursor to the end
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_textEdit->setTextCursor(cursor);
    
    // Add title if present
    if (!title.isEmpty()) {
        QString escapedTitle = title.toHtmlEscaped();
        m_textEdit->textCursor().insertHtml(
            QString("<div style='font-weight:bold; font-size:1.2em; color:#2c5aa0; "
                    "margin:15px 0 8px 0; padding:8px 0; border-bottom:2px solid #2c5aa0'>%1</div>")
            .arg(escapedTitle));
    }
    
    // Add content with overall styling
    m_textEdit->textCursor().insertHtml(
        QString("<div style='margin:5px 0 15px 0; padding:0; line-height:1.4'>%1</div>")
        .arg(htmlContent));
    
    // Scroll to the bottom
    m_textEdit->ensureCursorVisible();
}

void ResultDisplayWidget::convertMarkdownToHtml(QString &htmlContent)
{
    // Process code blocks first to avoid them being processed by other patterns
    QRegularExpression codeBlockRegex(R"(```(\w+)?\s*\n(.*?)```)", 
                                     QRegularExpression::DotMatchesEverythingOption | 
                                     QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator codeBlockIter = codeBlockRegex.globalMatch(htmlContent);
    int offset = 0;
    while (codeBlockIter.hasNext()) {
        QRegularExpressionMatch match = codeBlockIter.next();
        QString language = match.captured(1);
        QString code = match.captured(2).trimmed();
        
        // Remove trailing whitespace from each line while preserving structure
        QStringList lines = code.split('\n');
        for (QString& line : lines) {
            line = line.trimmed();
        }
        code = lines.join("\n");
        
        // Escape HTML special characters in code to prevent injection, but preserve formatting
        code = code.toHtmlEscaped();
        
        QString htmlCodeBlock = QString(
            "<div style='background-color:#2d2d2d; color:#dcdcdc; padding:10px; margin:5px 0; "
            "border-radius:5px; font-family:monospace; white-space:pre-wrap; overflow-x:auto; "
            "border-left:3px solid #ffcc00'>"
            "<div style='font-weight:bold; color:#ffcc00; margin-bottom:5px; font-size:0.9em'>%1 Code:</div>"
            "<code style='color:#dcdcdc; font-family:monospace; line-height:1.4'>%2</code>"
            "</div>")
            .arg(language.isEmpty() ? "Code" : language.toUpper())
            .arg(code);
        
        htmlContent.replace(match.capturedStart() + offset, match.capturedLength(), htmlCodeBlock);
        offset += htmlCodeBlock.length() - match.capturedLength();
    }
    
    // Process inline code blocks
    QRegularExpression inlineCodeRegex(R"(`(.*?)`)");
    QRegularExpressionMatchIterator inlineCodeIter = inlineCodeRegex.globalMatch(htmlContent);
    offset = 0;
    while (inlineCodeIter.hasNext()) {
        QRegularExpressionMatch match = inlineCodeIter.next();
        QString code = match.captured(1);
        QString escapedCode = code.toHtmlEscaped();
        QString htmlInlineCode = QString(
            "<code style='background-color:#f0f0f0; padding:2px 4px; "
            "border-radius:3px; font-family:monospace; font-weight:bold'>%1</code>")
            .arg(escapedCode);
        
        htmlContent.replace(match.capturedStart() + offset, match.capturedLength(), htmlInlineCode);
        offset += htmlInlineCode.length() - match.capturedLength();
    }
    
    // Process headers: # Header, ## Header, etc.
    QRegularExpression headerRegex(R"(^(#+)\s+(.*)$)", QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator headerIter = headerRegex.globalMatch(htmlContent);
    offset = 0;
    while (headerIter.hasNext()) {
        QRegularExpressionMatch match = headerIter.next();
        QString hashes = match.captured(1);
        QString header = match.captured(2);
        int level = qMin(hashes.length(), 6); // Limit to h6
        
        QString escapedHeader = header.toHtmlEscaped();
        QString htmlHeader = QString(
            "<h%1 style='margin:15px 0 10px 0; color:#2565ae; font-weight:bold; "
            "border-bottom:%2px solid #e0e0e0; padding-bottom:3px'>%3</h%1>")
            .arg(level)
            .arg(level > 3 ? 1 : 2)  // Smaller headers get thinner underline
            .arg(escapedHeader);
        
        htmlContent.replace(match.capturedStart() + offset, match.capturedLength(), htmlHeader);
        offset += htmlHeader.length() - match.capturedLength();
    }
    
    // Process bold: **text** or __text__
    QRegularExpression boldRegex(R"(\*{2}([^*]+?)\*{2}|_{2}([^_]+?)_{2})");
    QRegularExpressionMatchIterator boldIter = boldRegex.globalMatch(htmlContent);
    offset = 0;
    while (boldIter.hasNext()) {
        QRegularExpressionMatch match = boldIter.next();
        QString boldText = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
        QString escapedBoldText = boldText.toHtmlEscaped();
        QString htmlBold = QString("<strong style='font-weight:bold'>%1</strong>")
                           .arg(escapedBoldText);
        
        htmlContent.replace(match.capturedStart() + offset, match.capturedLength(), htmlBold);
        offset += htmlBold.length() - match.capturedLength();
    }
    
    // Process italic: *text* or _text_
    QRegularExpression italicRegex(R"((?<![\*_])\*([^*]+?)\*(?![\*_]|\s\*)|(?<![_\*])_([^_]+?)_(?![_\*]|\s_))");
    QRegularExpressionMatchIterator italicIter = italicRegex.globalMatch(htmlContent);
    offset = 0;
    while (italicIter.hasNext()) {
        QRegularExpressionMatch match = italicIter.next();
        QString italicText = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
        QString escapedItalicText = italicText.toHtmlEscaped();
        QString htmlItalic = QString("<em style='font-style:italic'>%1</em>")
                             .arg(escapedItalicText);
        
        htmlContent.replace(match.capturedStart() + offset, match.capturedLength(), htmlItalic);
        offset += htmlItalic.length() - match.capturedLength();
    }
    
    // Process bullet lists: - item
    QRegularExpression listRegex(R"((?:^|\n)\s*-\s+(.*?)(?=\n\s*-\s+|\n\s*\S|\n*$))");
    QRegularExpressionMatchIterator listIter = listRegex.globalMatch(htmlContent);
    offset = 0;
    while (listIter.hasNext()) {
        QRegularExpressionMatch match = listIter.next();
        QString item = match.captured(1).trimmed();
        QString escapedItem = item.toHtmlEscaped();
        QString htmlListItem = QString("<li style='margin:5px 0; line-height:1.4'>%1</li>")
                               .arg(escapedItem);
        
        htmlContent.replace(match.capturedStart() + offset, match.capturedLength(), QString("\n") + htmlListItem);
        offset += htmlListItem.length() - match.capturedLength() + 1; // +1 for the added \n
    }
    
    // Wrap the list items in <ul> tags
    htmlContent.replace(QRegularExpression(R"((\n\s*<li style='margin:5px 0; line-height:1.4'>.*?</li>)+)"), 
                        "<ul style='margin:5px 0 10px 20px; padding-left:20px; line-height:1.4'>\\1</ul>");
    
    // Process numbered lists: 1. item, 2. item, etc.
    QRegularExpression numListRegex(R"((?:^|\n)\s*(\d+)\.\s+(.*?)(?=\n\s*\d+\.|\n\s*\S|\n*$))");
    QRegularExpressionMatchIterator numListIter = numListRegex.globalMatch(htmlContent);
    offset = 0;
    while (numListIter.hasNext()) {
        QRegularExpressionMatch match = numListIter.next();
        QString number = match.captured(1);
        QString item = match.captured(2).trimmed();
        QString escapedItem = item.toHtmlEscaped();
        QString htmlListItem = QString("<li style='margin:5px 0; line-height:1.4'>%1</li>")
                               .arg(escapedItem);
        
        htmlContent.replace(match.capturedStart() + offset, match.capturedLength(), QString("\n") + htmlListItem);
        offset += htmlListItem.length() - match.capturedLength() + 1; // +1 for the added \n
    }
    
    // Wrap the numbered list items in <ol> tags
    htmlContent.replace(QRegularExpression(R"((\n\s*<li style='margin:5px 0; line-height:1.4'>.*?</li>)+)"), 
                        "<ol style='margin:5px 0 10px 20px; padding-left:20px; line-height:1.4'>\\1</ol>");
    
    // Convert remaining line breaks to HTML, but be careful not to break already formatted blocks
    htmlContent.replace(QRegularExpression(R"((?<!<br>)(?<!<br />)\n(?!\s*</))"), "<br>");
}