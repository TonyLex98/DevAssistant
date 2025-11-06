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

QString ResultDisplayWidget::processTable(const QStringList &tableRows)
{
    if (tableRows.isEmpty()) return QString();
    
    QStringList rowsHtml;
    
    // Find the header row (typically the first row before the separator)
    int headerRowIndex = -1;
    int separatorRowIndex = -1;
    
    for (int i = 0; i < tableRows.size(); i++) {
        QString line = tableRows[i].trimmed();
        if (line.startsWith("|") && line.contains("--") && line.contains("|")) {
            separatorRowIndex = i;
            break;
        }
    }
    
    if (separatorRowIndex > 0) {
        headerRowIndex = separatorRowIndex - 1;
    }
    
    // Process each row
    for (int i = 0; i < tableRows.size(); i++) {
        QString line = tableRows[i].trimmed();
        
        // Skip separator line
        if (line.startsWith("|") && line.contains("--") && line.contains("|")) {
            continue;
        }
        
        if (line.startsWith("|") && line.endsWith("|")) {
            // This is a table row
            QString content = line.mid(1, line.length() - 2); // Remove leading and trailing |
            
            // Split by | but avoid splitting inside code blocks
            QStringList cells;
            int lastPos = 0;
            int pos = 0;
            
            while (pos < content.length()) {
                if (content[pos] == '|' && 
                    (pos == 0 || content[pos-1] != '\\')) { // Not escaped
                    cells << content.mid(lastPos, pos - lastPos).trimmed();
                    lastPos = pos + 1;
                }
                pos++;
            }
            cells << content.mid(lastPos).trimmed(); // Add last cell
            
            // Determine if this is header row
            bool isHeaderRow = (i == headerRowIndex);
            QString rowTag = isHeaderRow ? "th" : "td";
            QString rowStyle = isHeaderRow ? "style='background-color: #f2f2f2; font-weight: bold'" : "";
            
            QString rowHtml = QString("<tr>");
            for (const QString &cell : cells) {
                QString escapedCell = cell.toHtmlEscaped();
                rowHtml += QString("<%1 %2 style='border: 1px solid #ddd; padding: 8px; text-align: left'>%3</%1>")
                           .arg(rowTag, rowStyle, escapedCell);
            }
            rowHtml += QString("</tr>");
            
            rowsHtml << rowHtml;
        }
    }
    
    if (rowsHtml.isEmpty()) return QString();
    
    return QString("<table style='border-collapse: collapse; width: 100%%; margin: 10px 0; border: 1px solid #ddd; font-family: Arial, sans-serif; font-size: 14px'>%1</table>")
             .arg(rowsHtml.join(""));
}

void ResultDisplayWidget::convertMarkdownToHtml(QString &htmlContent)
{
    // Create a temporary storage for already processed HTML blocks to avoid double processing
    struct Placeholder {
        QString original;
        QString placeholder;
    };
    
    QVector<Placeholder> placeholders;
    int placeholderCounter = 0;
    
    // Step 1: Process code blocks first and temporarily replace them with placeholders
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
            .arg(language.isEmpty() ? "Code" : language.toUpper()).arg(code);
        
        QString placeholder = QString("MARKDOWN_CODEBLOCK_%1_TEMP_PLACEHOLDER").arg(placeholderCounter);
        placeholders.append({htmlCodeBlock, placeholder});
        
        htmlContent.replace(match.capturedStart() + offset, match.capturedLength(), placeholder);
        offset += placeholder.length() - match.capturedLength();
        placeholderCounter++;
    }
    
    // Step 2: Process inline code blocks and temporarily replace them with placeholders
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
        
        QString placeholder = QString("MARKDOWN_INLINE_CODE_%1_TEMP_PLACEHOLDER").arg(placeholderCounter);
        placeholders.append({htmlInlineCode, placeholder});
        
        htmlContent.replace(match.capturedStart() + offset, match.capturedLength(), placeholder);
        offset += placeholder.length() - match.capturedLength();
        placeholderCounter++;
    }
    
    // Step 3: Process headers: # Header, ## Header, etc.
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
    
    // Step 4: Process bold: **text** (only asterisks, not underscores to avoid conflicts with code)
    QRegularExpression boldRegex(R"(\*{2}([^*]+?)\*{2})");
    QRegularExpressionMatchIterator boldIter = boldRegex.globalMatch(htmlContent);
    offset = 0;
    while (boldIter.hasNext()) {
        QRegularExpressionMatch match = boldIter.next();
        QString boldText = match.captured(1);
        QString escapedBoldText = boldText.toHtmlEscaped();
        QString htmlBold = QString("<strong style='font-weight:bold'>%1</strong>")
                           .arg(escapedBoldText);
        
        htmlContent.replace(match.capturedStart() + offset, match.capturedLength(), htmlBold);
        offset += htmlBold.length() - match.capturedLength();
    }
    
    // Step 5: Process italic: *text* (only asterisks, not underscores to avoid conflicts with code)
    QRegularExpression italicRegex(R"((?<!\*)\*([^*]+?)\*(?!\*))");
    QRegularExpressionMatchIterator italicIter = italicRegex.globalMatch(htmlContent);
    offset = 0;
    while (italicIter.hasNext()) {
        QRegularExpressionMatch match = italicIter.next();
        QString italicText = match.captured(1);
        QString escapedItalicText = italicText.toHtmlEscaped();
        QString htmlItalic = QString("<em style='font-style:italic'>%1</em>")
                             .arg(escapedItalicText);
        
        htmlContent.replace(match.capturedStart() + offset, match.capturedLength(), htmlItalic);
        offset += htmlItalic.length() - match.capturedLength();
    }
    
    // Step 6: Process bullet lists: - item
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
    
    // Step 7: Process numbered lists: 1. item, 2. item, etc.
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
    
    // Step 8: Process tables: | header1 | header2 | header3 |
    // First, let's find and process any markdown tables
    // Find table blocks - they start with |, have separator line with dashes, and continue with rows
    QStringList allLines = htmlContent.split('\n');
    QStringList resultLines;
    bool inTable = false;
    QStringList tableRows;
    
    for (int i = 0; i < allLines.size(); i++) {
        QString line = allLines[i];
        
        if (line.trimmed().startsWith("|") && line.trimmed().contains("--") && line.trimmed().contains("|")) {
            // This is a separator line, the line before it is the header
            if (i > 0 && allLines[i-1].trimmed().startsWith("|")) {
                // This confirms it's a table - process the table
                tableRows << line; // Add the separator line
                inTable = true;
            } else {
                resultLines << line; // Not part of a table, just add to results
            }
        } else if (line.trimmed().startsWith("|") && inTable) {
            // Continue adding table rows
            tableRows << line;
        } else if (line.trimmed().startsWith("|") && !inTable) {
            // Potential start of a table - check if next line is separator
            if (i + 1 < allLines.size()) {
                QString nextLine = allLines[i + 1];
                if (nextLine.trimmed().startsWith("|") && nextLine.contains("--") && nextLine.contains(":")) {
                    // Confirmed: this is a table
                    tableRows << line << nextLine; // Add current line and separator
                    inTable = true;
                    i++; // Skip the next line (separator) since we added it
                    continue;
                } else {
                    resultLines << line; // Not a table, just add to results
                }
            } else {
                resultLines << line; // Last line, add as normal
            }
        } else if (inTable && (!line.trimmed().startsWith("|") || line.trimmed().isEmpty())) {
            // End of table, process it and add to result
            QString tableHtml = processTable(tableRows);
            resultLines << tableHtml;
            tableRows.clear();
            inTable = false;
            resultLines << line; // Add current non-table line
        } else {
            if (inTable) {
                tableRows << line; // Add to table if we're still in table mode
            } else {
                resultLines << line; // Regular line
            }
        }
    }
    
    // If we ended with a table, process it
    if (inTable && !tableRows.isEmpty()) {
        QString tableHtml = processTable(tableRows);
        resultLines << tableHtml;
        tableRows.clear();
    }
    
    htmlContent = resultLines.join('\n');
    
    // Step 9: Convert remaining line breaks to HTML, but be careful not to break already formatted blocks
    htmlContent.replace(QRegularExpression(R"((?<!<br>)(?<!<br />)\n(?!\s*</))"), "<br>");
    
    // Step 10: Restore the temporarily replaced HTML blocks
    // Process in reverse order to avoid issues with offset changes during replacement
    for (int i = placeholders.size() - 1; i >= 0; --i) {
        const auto& placeholder = placeholders[i];
        htmlContent.replace(placeholder.placeholder, placeholder.original);
    }
}
