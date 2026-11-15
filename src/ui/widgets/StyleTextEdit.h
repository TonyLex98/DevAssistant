// StyleTextEdit.h
#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextEdit>

class TemplateSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit TemplateSyntaxHighlighter(QTextDocument *parent = nullptr)
        : QSyntaxHighlighter(parent)
    {
        m_openSequenceInfoFormat.setForeground(QColor(190, 24, 93));
        m_openSequenceInfoFormat.setFontWeight(QFont::Bold);
    }

protected:
    void highlightBlock(const QString &text) override
    {
        static QRegularExpression openSeqInfoRegex(R"(\{\{OPEN_SEQUENCE_INFO\}\})");
        QRegularExpressionMatchIterator iter = openSeqInfoRegex.globalMatch(text);
        while (iter.hasNext()) {
            const QRegularExpressionMatch match = iter.next();
            setFormat(match.capturedStart(), match.capturedLength(), m_openSequenceInfoFormat);
        }
    }

private:
    QTextCharFormat m_openSequenceInfoFormat;
};

class StyleTextEdit : public QTextEdit
{
    Q_OBJECT
public:
    explicit StyleTextEdit(QWidget *parent = nullptr)
        : QTextEdit(parent)
    {
        m_syntaxHighlighter = new TemplateSyntaxHighlighter(this->document());

        setStyleSheet(
            "QTextEdit {"
            "  border: 1px solid #0f766e;"
            "  border-radius: 6px;"
            "  padding: 8px 10px;"
            "  font: 10pt \"Consolas\";"
            "  background: #ffffff;"
            "  color: #0f172a;"
            "}"
            "QTextEdit:focus { border-color: #14b8a6; background: #ecfeff; }"
        );

        setAcceptRichText(false);
        setMaximumHeight(QWIDGETSIZE_MAX);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }

    void setPlainText(const QString &text)
    {
        QTextEdit::setPlainText(text);
        document()->setDocumentMargin(2);
    }

protected:
    TemplateSyntaxHighlighter* m_syntaxHighlighter = nullptr;
};
