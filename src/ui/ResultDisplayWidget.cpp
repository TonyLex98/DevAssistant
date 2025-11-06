#include "ResultDisplayWidget.h"
#include <QTextCursor>
#include <QTextBlock>
#include "../../src/md4c/src/md4c-html.h"

// Global function for md4c callback to avoid lambda conversion issues
static void md4cOutputCallback(const MD_CHAR* str, MD_SIZE size, void* userdata) {
    QByteArray* buffer = static_cast<QByteArray*>(userdata);
    buffer->append(str, static_cast<int>(size));
}

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
    // Convert QString to UTF-8 QByteArray for md4c
    QByteArray markdownBytes = htmlContent.toUtf8();
    const char* markdownText = markdownBytes.constData();
    MD_SIZE markdownSize = static_cast<MD_SIZE>(markdownBytes.size());
    
    // Buffer to collect HTML output
    QByteArray htmlOutput;
    
    // Call md4c to convert Markdown to HTML
    int result = md_html(
        reinterpret_cast<const MD_CHAR*>(markdownText),
        markdownSize,
        md4cOutputCallback,  // Use the global callback function
        &htmlOutput,         // userdata  
        MD_FLAGS_CHATGPT_STYLE,//MD_DIALECT_GITHUB,   // parser flags - using GitHub dialect (includes tables)
        0  // renderer flags
    );
    
    if (result == 0) {
        // Conversion successful, update the content
        htmlContent = QString::fromUtf8(htmlOutput);
    }
    // If conversion failed, keep the original content
}
