#include "RagWidget.h"

#include "RagIndex.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace Rag {

RagWidget::RagWidget(QWidget* parent)
    : QWidget(parent), m_index(new Index(this))
{
    setWindowTitle(tr("Documents: search and questions"));
    resize(900, 700);

    auto* layout = new QVBoxLayout(this);

    // Index settings.
    auto* form = new QFormLayout;
    auto* folderRow = new QHBoxLayout;
    m_folder = new QLineEdit(this);
    auto* browse = new QPushButton(tr("Browse..."), this);
    folderRow->addWidget(m_folder, 1);
    folderRow->addWidget(browse);
    form->addRow(tr("Folder"), folderRow);

    QSettings settings;
    m_embeddingModel = new QLineEdit(settings.value("rag/embeddingModel", "nomic-embed-text").toString(), this);
    m_embeddingModel->setToolTip(tr("An embedding model on the same server as the chat model, "
                                    "for example nomic-embed-text or bge-m3 (ollama pull nomic-embed-text)."));
    form->addRow(tr("Embedding model"), m_embeddingModel);

    m_topK = new QSpinBox(this);
    m_topK->setRange(1, 30);
    m_topK->setValue(settings.value("rag/topK", 6).toInt());
    form->addRow(tr("Excerpts per answer"), m_topK);
    layout->addLayout(form);

    auto* indexRow = new QHBoxLayout;
    m_indexButton = new QPushButton(tr("Index"), this);
    m_indexButton->setToolTip(tr("Reads the folder's text, code and PDF files. Only changed files are "
                                 "processed again."));
    m_stopButton = new QPushButton(tr("Stop"), this);
    m_progress = new QProgressBar(this);
    indexRow->addWidget(m_indexButton);
    indexRow->addWidget(m_stopButton);
    indexRow->addWidget(m_progress, 1);
    layout->addLayout(indexRow);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_status);

    // Question and answer.
    auto* askRow = new QHBoxLayout;
    m_question = new QLineEdit(this);
    m_question->setPlaceholderText(tr("Ask about these documents"));
    m_askButton = new QPushButton(tr("Ask"), this);
    askRow->addWidget(m_question, 1);
    askRow->addWidget(m_askButton);
    layout->addLayout(askRow);

    auto* split = new QSplitter(Qt::Vertical, this);
    m_answer = new QTextBrowser(split);
    m_answer->setOpenExternalLinks(false);
    m_sources = new QListWidget(split);
    m_sources->setToolTip(tr("Double-click to open the file"));
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    layout->addWidget(split, 1);

    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Folder to index"), m_folder->text());
        if (!dir.isEmpty())
            setFolder(dir);
    });
    connect(m_folder, &QLineEdit::editingFinished, this, &RagWidget::updateIndexInfo);
    connect(m_embeddingModel, &QLineEdit::editingFinished, this, [this]() {
        QSettings().setValue("rag/embeddingModel", m_embeddingModel->text().trimmed());
    });
    connect(m_topK, QOverload<int>::of(&QSpinBox::valueChanged), this, [](int k) {
        QSettings().setValue("rag/topK", k);
    });
    connect(m_indexButton, &QPushButton::clicked, this, &RagWidget::startIndexing);
    connect(m_stopButton, &QPushButton::clicked, m_index, &Index::cancel);
    connect(m_askButton, &QPushButton::clicked, this, &RagWidget::ask);
    connect(m_question, &QLineEdit::returnPressed, this, &RagWidget::ask);
    connect(m_sources, &QListWidget::itemDoubleClicked, this, [](QListWidgetItem* item) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(item->data(Qt::UserRole).toString()));
    });

    connect(m_index, &Index::progress, this, [this](int done, int total, const QString& file) {
        m_progress->setRange(0, qMax(1, total));
        m_progress->setValue(done);
        if (!file.isEmpty())
            m_status->setText(tr("Indexing %1 (%2 of %3)").arg(file).arg(done + 1).arg(total));
    });
    connect(m_index, &Index::indexingFinished, this, [this](const QString& summary) {
        setBusy(false);
        m_status->setText(summary.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));
    });
    connect(m_index, &Index::answerReady, this, [this](const QString& answer, const QVector<Hit>& sources) {
        setBusy(false);
        showAnswer(answer, sources);
    });
    connect(m_index, &Index::failed, this, [this](const QString& error) {
        setBusy(false);
        m_status->setText(QStringLiteral("<span style='color:#b91c1c'>%1</span>").arg(error.toHtmlEscaped()));
    });

    setBusy(false);
}

void RagWidget::setFolder(const QString& folder)
{
    m_folder->setText(QDir::toNativeSeparators(folder));
    updateIndexInfo();
}

void RagWidget::updateIndexInfo()
{
    const QString folder = QDir::fromNativeSeparators(m_folder->text().trimmed());
    if (folder.isEmpty()) {
        m_status->setText(tr("Choose a folder to index."));
        return;
    }
    m_status->setText(QFileInfo::exists(Index::databasePathFor(folder))
                      ? tr("This folder has an index. Index again to pick up changes, or ask a question.")
                      : tr("This folder is not indexed yet. Make sure the embedding model is downloaded, then press Index."));
}

void RagWidget::setBusy(bool busy)
{
    m_indexButton->setEnabled(!busy);
    m_askButton->setEnabled(!busy);
    m_stopButton->setEnabled(busy);
    if (!busy) {
        m_progress->setRange(0, 1);
        m_progress->setValue(0);
    }
}

void RagWidget::startIndexing()
{
    const QString folder = QDir::fromNativeSeparators(m_folder->text().trimmed());
    if (folder.isEmpty() || !QFileInfo(folder).isDir()) {
        m_status->setText(tr("The folder does not exist."));
        return;
    }
    QSettings().setValue("rag/embeddingModel", m_embeddingModel->text().trimmed());
    setBusy(true);
    m_progress->setRange(0, 0);
    m_status->setText(tr("Looking for files..."));
    m_index->startIndexing(folder);
}

void RagWidget::ask()
{
    const QString folder = QDir::fromNativeSeparators(m_folder->text().trimmed());
    const QString question = m_question->text().trimmed();
    if (question.isEmpty() || m_index->isBusy())
        return;
    setBusy(true);
    m_progress->setRange(0, 0);
    m_status->setText(tr("Searching and asking the model..."));
    m_answer->clear();
    m_sources->clear();
    m_index->startAsking(folder, question);
}

void RagWidget::showAnswer(const QString& answer, const QVector<Hit>& sources)
{
    m_status->setText(tr("Answer from %n excerpt(s).", nullptr, sources.size()));
    m_answer->setMarkdown(answer);
    for (int i = 0; i < sources.size(); ++i) {
        const Hit& h = sources.at(i);
        QString preview = h.chunk.text.simplified();
        if (preview.size() > 160)
            preview = preview.left(160) + QStringLiteral("…");
        auto* item = new QListWidgetItem(QStringLiteral("[%1] %2, part %3 (%4)\n%5")
                                         .arg(i + 1)
                                         .arg(QDir::toNativeSeparators(h.chunk.path))
                                         .arg(h.chunk.ordinal + 1)
                                         .arg(QString::number(h.score, 'f', 2), preview),
                                         m_sources);
        item->setData(Qt::UserRole, h.chunk.path);
        item->setToolTip(h.chunk.text.left(2000));
    }
}

} // namespace Rag
