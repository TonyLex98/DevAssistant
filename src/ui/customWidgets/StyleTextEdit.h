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
        // Format for OPEN_SEQUENCE commands
        m_openSequenceFormat.setForeground(QColor(255, 105, 180)); // Hot pink color
        m_openSequenceFormat.setFontWeight(QFont::Bold);
    }

protected:
    void highlightBlock(const QString &text) override
    {
        // Highlight OPEN_SEQUENCE commands: {{OPEN_SEQUENCE:...}}
        QRegularExpression openSeqRegex(R"(\{\{OPEN_SEQUENCE:[^}]*\}\})");
        QRegularExpressionMatchIterator iter = openSeqRegex.globalMatch(text);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            setFormat(match.capturedStart(), match.capturedLength(), m_openSequenceFormat);
        }
    }

private:
    QTextCharFormat m_openSequenceFormat;
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
        qDebug() << "[StyleTextEdit] ctor, parent =" << parent;
    }

protected:
    TemplateSyntaxHighlighter* m_syntaxHighlighter = nullptr;

};
