#include "ResultDisplayWidget.h"
#include "../../src/md4c/src/md4c-html.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTextDocument>
#include <QStringList>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <algorithm>
#include <QSet>

static void md4cOutputCallback(const MD_CHAR* str, MD_SIZE size, void* userdata) {
    QByteArray* buffer = static_cast<QByteArray*>(userdata);
    buffer->append(str, static_cast<int>(size));
}

ResultDisplayWidget::ResultDisplayWidget(QWidget *parent)
    : QWidget(parent),
      m_scrollArea(new QScrollArea(this)),
      m_contentWidget(new QWidget),
      m_layout(new QVBoxLayout)
{
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_layout->setSpacing(12);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_contentWidget->setLayout(m_layout);
    m_scrollArea->setWidget(m_contentWidget);

    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(m_scrollArea);
    setLayout(outerLayout);
}

void ResultDisplayWidget::appendResult(const QString &title,
                                       const QString &content,
                                       EntryType type,
                                       const ResultEntryMetadata &metadata)
{
    QString htmlContent = content;
    QString sourceText = content;
    convertMarkdownToHtml(htmlContent);

    QWidget *entry = createEntryWidget(title, htmlContent, sourceText, type, metadata);
    m_layout->addWidget(entry);

    scrollToBottom();
}

QWidget *ResultDisplayWidget::createEntryWidget(const QString &title,
                                                const QString &htmlContent,
                                                const QString &sourceText,
                                                EntryType type,
                                                const ResultEntryMetadata &metadata)
{
    QWidget *entry = new QWidget(m_contentWidget);
    entry->setObjectName("resultEntry");
    entry->setStyleSheet(QString(
        "#resultEntry {"
        "  background-color: %1;"
        "  border: 1px solid #dcdcdc;"
        "  border-radius: 6px;"
        "}"
    ).arg(type == EntryType::UserQuestion ? "#f2f7ff" : "#ffffff"));
    entry->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

    QVBoxLayout *layout = new QVBoxLayout(entry);
    layout->setSpacing(8);
    layout->setContentsMargins(12, 12, 12, 12);

    if (!title.isEmpty()) {
        QLabel *titleLabel = new QLabel(title.toHtmlEscaped(), entry);
        titleLabel->setStyleSheet("font-weight: bold; color: #2c5aa0; font-size: 1.3em;");
        titleLabel->setWordWrap(true);
        layout->addWidget(titleLabel);
    }

    QLabel *snippetLabel = nullptr;
    QPushButton *reasoningToggle = nullptr;
    if (type == EntryType::Reasoning) {
        reasoningToggle = new QPushButton(tr("Показать рассуждение"), entry);
        reasoningToggle->setCheckable(true);
        reasoningToggle->setChecked(false);
        reasoningToggle->setFlat(true);
        reasoningToggle->setStyleSheet("text-align: left; color: #2c5aa0; font-weight: bold;");
        layout->addWidget(reasoningToggle);

        snippetLabel = new QLabel(reasoningSnippet(sourceText), entry);
        snippetLabel->setWordWrap(true);
        snippetLabel->setStyleSheet("color: #555;");
        layout->addWidget(snippetLabel);
    }

    QTextBrowser *browser = new QTextBrowser(entry);
    browser->setFrameShape(QFrame::NoFrame);
    browser->setOpenExternalLinks(true);
    browser->setContextMenuPolicy(Qt::NoContextMenu);
    browser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    browser->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    browser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    browser->setStyleSheet(
        "QTextBrowser {"
        "  border: none;"
        "  padding: 0;"
        "  font-family: Arial, sans-serif;"
        "  font-size: 14px;"
        "  line-height: 1.4;"
        "}"
        "table {"
        "  border-collapse: collapse;"
        "  width: 100%;"
        "  margin: 8px 0;"
        "}"
        "th, td {"
        "  border: 1px solid #ddd;"
        "  padding: 8px;"
        "  text-align: left;"
        "}"
        "th {"
        "  background-color: #f4f4f4;"
        "}"
        "pre {"
        "  background-color: #f8f8f8;"
        "  padding: 8px;"
        "  border-radius: 4px;"
        "}"
    );
    browser->setHtml(htmlContent.isEmpty() ? tr("<i>Нет содержимого</i>") : htmlContent);
    browser->document()->adjustSize();
    const int docHeight = static_cast<int>(browser->document()->size().height());
    browser->setMinimumHeight(docHeight + browser->document()->documentMargin() * 2 + 12);
    layout->addWidget(browser);

    if (reasoningToggle && snippetLabel) {
        browser->setVisible(false);
        QObject::connect(reasoningToggle, &QPushButton::toggled, entry, [browser, reasoningToggle, snippetLabel](bool checked) {
            browser->setVisible(checked);
            snippetLabel->setVisible(!checked);
            reasoningToggle->setText(checked ? QObject::tr("Скрыть рассуждение")
                                              : QObject::tr("Показать рассуждение"));
        });
    }

    if (!metadata.commands.isEmpty() || !metadata.sequences.isEmpty()) {
        QPushButton *contextToggle = new QPushButton(tr("Показать команды и последовательности"), entry);
        contextToggle->setCheckable(true);
        contextToggle->setChecked(false);
        contextToggle->setFlat(true);
        contextToggle->setStyleSheet("text-align: left; color: #2c5aa0; font-weight: bold;");
        layout->addWidget(contextToggle);

        QTreeWidget *contextTree = new QTreeWidget(entry);
        contextTree->setHeaderLabels(QStringList() << tr("Время") << tr("Описание"));
        contextTree->setRootIsDecorated(true);
        contextTree->setItemsExpandable(true);
        contextTree->setIndentation(12);
        contextTree->setColumnCount(2);
        contextTree->setSelectionMode(QAbstractItemView::NoSelection);
        contextTree->setRootIsDecorated(true);
        contextTree->setFrameShape(QFrame::NoFrame);
        contextTree->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        contextTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        contextTree->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        contextTree->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        contextTree->setVisible(false);
        contextTree->setStyleSheet(
            "QTreeWidget { background-color: #fbfbfb; border: 1px solid #e0e0e0; }"
            "QTreeWidget::item { padding: 4px 8px; }"
        );
        contextTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        contextTree->header()->setStretchLastSection(true);

        populateContextTree(contextTree, metadata);
        layout->addWidget(contextTree);

        QObject::connect(contextToggle, &QPushButton::toggled, entry, [contextTree, contextToggle](bool checked) {
            contextTree->setVisible(checked);
            contextToggle->setText(checked ? QObject::tr("Скрыть команды и последовательности")
                                            : QObject::tr("Показать команды и последовательности"));
        });
    }

    QString plainText = browser->document()->toPlainText();
    QString trimmedSource = sourceText.trimmed();

    QWidget *buttonRow = new QWidget(entry);
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 2, 0, 0);
    buttonLayout->setSpacing(8);

    auto addCopyButton = [&](const QString &label, const QString &value) {
        if (value.trimmed().isEmpty())
            return;
        QPushButton *btn = new QPushButton(label, buttonRow);
        btn->setFlat(true);
        btn->setStyleSheet("color: #2c5aa0; font-weight: bold; padding: 4px 12px;");
        QObject::connect(btn, &QPushButton::clicked, entry, [value]() {
            QApplication::clipboard()->setText(value);
        });
        buttonLayout->addWidget(btn);
    };

    if (type == EntryType::UserQuestion) {
        addCopyButton(tr("Копировать вопрос"), trimmedSource);
    } else {
        addCopyButton(tr("Копировать ответ"), plainText);
    }

    if (type == EntryType::Reasoning) {
        addCopyButton(tr("Копировать рассуждение"), trimmedSource);
    }

    if (buttonLayout->count() > 0) {
        layout->addWidget(buttonRow);
    }

    QFrame *separator = new QFrame(entry);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    return entry;
}

void ResultDisplayWidget::populateContextTree(QTreeWidget *tree, const ResultEntryMetadata &metadata)
{
    tree->clear();

    QSet<QString> addedCommands;

    for (const auto &seq : metadata.sequences) {
        QTreeWidgetItem *seqItem = new QTreeWidgetItem(tree);
        seqItem->setText(0, seq.timestamp.toString("hh:mm:ss"));
        seqItem->setText(1, seq.name.isEmpty() ? tr("Последовательность без имени") : seq.name);
        seqItem->setBackground(0, QColor(255, 235, 205));
        seqItem->setBackground(1, QColor(255, 235, 205));

        for (const QString &cmdId : seq.commandIds) {
            for (const auto &cmd : metadata.commands) {
                if (cmd.id == cmdId) {
                    addCommandItem(tree, seqItem, cmd, addedCommands);
                    break;
                }
            }
        }
    }

    for (const auto &cmd : metadata.commands) {
        if (!addedCommands.contains(cmd.id)) {
            addCommandItem(tree, nullptr, cmd, addedCommands);
        }
    }

    if (tree->topLevelItemCount() == 0) {
        QTreeWidgetItem *empty = new QTreeWidgetItem(tree);
        empty->setText(0, "");
        empty->setText(1, tr("Нет применённых команд"));
        empty->setDisabled(true);
    } else {
        tree->expandToDepth(0);
    }

    int totalRows = 0;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        totalRows += 1;
        totalRows += tree->topLevelItem(i)->childCount();
    }
    int rowHeight = tree->sizeHintForRow(0);
    if (rowHeight <= 0)
        rowHeight = tree->fontMetrics().height() + 4;
    int desiredRows = std::max(1, totalRows);
    int desiredHeight = tree->header()->height() + desiredRows * rowHeight + 4;
    tree->setFixedHeight(desiredHeight);
}

void ResultDisplayWidget::addCommandItem(QTreeWidget *tree,
                                        QTreeWidgetItem *parent,
                                        const CommandContextInfo &cmd,
                                        QSet<QString> &addedCommands)
{
    QTreeWidgetItem *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
    item->setText(0, cmd.timestamp.toString("hh:mm:ss"));
    QString desc = cmd.description.isEmpty() ? cmd.name : cmd.description;
    item->setText(1, desc);

    QColor bgColor;
    switch (cmd.state) {
        case CommandState::Active:
            bgColor = QColor(200, 255, 200);
            item->setForeground(0, Qt::darkGreen);
            item->setForeground(1, Qt::darkGreen);
            break;
        case CommandState::Invalidated:
            bgColor = QColor(255, 220, 220);
            item->setForeground(0, Qt::red);
            item->setForeground(1, Qt::red);
            break;
        case CommandState::Removed:
            bgColor = QColor(210, 210, 210);
            item->setForeground(0, Qt::gray);
            item->setForeground(1, Qt::gray);
            break;
    }

    if (bgColor.isValid()) {
        item->setBackground(0, bgColor);
        item->setBackground(1, bgColor);
    }

    addedCommands.insert(cmd.id);

    if (!cmd.result.isEmpty()) {
        QTreeWidgetItem *resultItem = new QTreeWidgetItem(item);
        resultItem->setText(0, tr("Результат"));
        resultItem->setText(1, cmd.result);
        resultItem->setBackground(0, QColor(245, 245, 245));
        resultItem->setBackground(1, QColor(245, 245, 245));
        resultItem->setForeground(0, Qt::darkBlue);
        resultItem->setForeground(1, Qt::darkBlue);
    }
}

QString ResultDisplayWidget::reasoningSnippet(const QString &source) const
{
    const QString trimmed = source.trimmed();
    const int limit = 200;
    if (trimmed.length() <= limit)
        return trimmed;
    return trimmed.left(limit) + QStringLiteral("…");
}

void ResultDisplayWidget::scrollToBottom()
{
    if (!m_scrollArea)
        return;

    QScrollBar *bar = m_scrollArea->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void ResultDisplayWidget::convertMarkdownToHtml(QString &htmlContent)
{
    QByteArray markdownBytes = htmlContent.toUtf8();
    const char* markdownText = markdownBytes.constData();
    MD_SIZE markdownSize = static_cast<MD_SIZE>(markdownBytes.size());

    QByteArray htmlOutput;

    int result = md_html(
        reinterpret_cast<const MD_CHAR*>(markdownText),
        markdownSize,
        md4cOutputCallback,
        &htmlOutput,
        MD_FLAGS_CHATGPT_STYLE,
        0
    );

    if (result == 0) {
        htmlContent = QString::fromUtf8(htmlOutput);
    }
}
