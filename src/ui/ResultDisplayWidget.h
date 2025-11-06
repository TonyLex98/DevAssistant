#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QRegularExpression>

class ResultDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ResultDisplayWidget(QWidget *parent = nullptr);

    void appendResult(const QString &title, const QString &content);

private:
    QTextEdit *m_textEdit;
    void convertMarkdownToHtml(QString &htmlContent);
    QString processTable(const QStringList &tableRows);
};