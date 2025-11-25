// StyleTextEdit.h
#pragma once

#include <QTextEdit>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QKeyEvent>
#include <QDebug>

// Syntax highlighter for special commands in templates
class TemplateSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    TemplateSyntaxHighlighter(QTextDocument *parent = nullptr) : QSyntaxHighlighter(parent)
    {
        // Format for OPEN_SEQUENCE_INFO
        m_openSequenceInfoFormat.setForeground(QColor(255, 105, 180)); // Hot pink color
        m_openSequenceInfoFormat.setFontWeight(QFont::Bold);
    }

protected:
    void highlightBlock(const QString &text) override
    {
        // Highlight OPEN_SEQUENCE_INFO: {{OPEN_SEQUENCE_INFO}}
        QRegularExpression openSeqInfoRegex(R"(\{\{OPEN_SEQUENCE_INFO\}\})");
        QRegularExpressionMatchIterator iter = openSeqInfoRegex.globalMatch(text);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            setFormat(match.capturedStart(), match.capturedLength(), m_openSequenceInfoFormat);
        }
    }

private:
    QTextCharFormat m_openSequenceInfoFormat;
};


/**
 * @brief StyleTextEdit
 *        QTextEdit, стилизованный как MemoryComboBox, с подсветкой синтаксиса.
 *
 * В конструкторе задаётся:
 *   - 2‑пиксельный серо‑синий бордер (#5A9)
 *   - 12‑пиксельное скругление углов
 *   - padding: 6px 12px
 *   - font: 18px "Arial", sans-serif
 *
 * При необходимости можно переопределить keyPressEvent
 * (например, чтобы посылать сообщение по Enter).
 */
class StyleTextEdit : public QTextEdit
{
    Q_OBJECT
public:
    explicit StyleTextEdit(QWidget *parent = nullptr)
        : QTextEdit(parent)
    {
        // Initialize syntax highlighter
        m_syntaxHighlighter = new TemplateSyntaxHighlighter(this->document());

        // --------- Стили, как у MemoryComboBox ----------
        setStyleSheet(
            "QTextEdit {"
            "  border: 2px solid #5A9;"        // 2px бордер, цвет #5A9
            "  border-radius: 12px;"          // скругление 12px
            "  padding: 6px 12px;"            // внутренние отступы
            "  font: 18px \"Arial\", sans-serif;"  // шрифт
            "}"
        );

        // Remove any size constraints to allow unlimited text
        setAcceptRichText(false);  // Use plain text to reduce overhead
        setMaximumHeight(QWIDGETSIZE_MAX);  // Remove height limit
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);  // Ensure scroll bar appears when needed

        qDebug() << "[StyleTextEdit] ctor, parent =" << parent;
    }

    // Ensure unlimited text by not limiting the document
    void setPlainText(const QString &text) {
        QTextEdit::setPlainText(text);
        // Adjust document size without limits
        document()->setDocumentMargin(2);
    }

protected:
    TemplateSyntaxHighlighter* m_syntaxHighlighter = nullptr;

};
