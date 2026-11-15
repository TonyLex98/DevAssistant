#include "LightRagWidget.h"
#include "LightRagClient.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGraphicsEllipseItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QDir>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cmath>

namespace {

// A graphics view that zooms with the mouse wheel.
class ZoomView : public QGraphicsView
{
public:
    using QGraphicsView::QGraphicsView;
protected:
    void wheelEvent(QWheelEvent* e) override
    {
        const double f = e->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        scale(f, f);
    }
};

QString nodeTitle(const QJsonObject& node)
{
    const QJsonObject props = node.value(QStringLiteral("properties")).toObject();
    const QString entity = props.value(QStringLiteral("entity_id")).toString();
    return entity.isEmpty() ? node.value(QStringLiteral("id")).toString() : entity;
}

} // namespace

LightRagWidget::LightRagWidget(QWidget* parent)
    : QWidget(parent), m_client(new LightRagClient(this))
{
    setWindowTitle(tr("LightRAG server"));
    resize(1000, 720);

    auto* layout = new QVBoxLayout(this);
    auto* serverRow = new QHBoxLayout;
    QSettings s;
    m_serverEdit = new QLineEdit(s.value("lightrag/server", "http://localhost:9621").toString(), this);
    m_apiKeyEdit = new QLineEdit(s.value("lightrag/apiKey").toString(), this);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText(tr("API key, if the server has one"));
    auto* connectButton = new QPushButton(tr("Connect"), this);
    serverRow->addWidget(new QLabel(tr("Server"), this));
    serverRow->addWidget(m_serverEdit, 2);
    serverRow->addWidget(m_apiKeyEdit, 1);
    serverRow->addWidget(connectButton);
    layout->addLayout(serverRow);

    m_status = new QLabel(tr("LightRAG runs as a separate server (pip install \"lightrag-hku[api]\", then lightrag-server). "
                             "It has its own LLM and embedding settings."), this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildDocumentsTab(), tr("Documents"));
    tabs->addTab(buildQueryTab(), tr("Ask"));
    tabs->addTab(buildGraphTab(), tr("Knowledge graph"));
    layout->addWidget(tabs, 1);

    connect(connectButton, &QPushButton::clicked, this, &LightRagWidget::connectToServer);

    connect(m_client, &LightRagClient::healthChecked, this, [this](bool ok, const QString& details) {
        if (ok) {
            setStatus(tr("Connected. %1").arg(details));
            m_client->listDocuments();
            m_client->listGraphLabels();
        } else {
            setStatus(tr("Cannot reach the server: %1").arg(details), true);
        }
    });
    connect(m_client, &LightRagClient::failed, this, [this](const QString& what, const QString& error) {
        setStatus(QStringLiteral("%1: %2").arg(what, error), true);
        m_askButton->setEnabled(true);
    });
    connect(m_client, &LightRagClient::documentsListed, this, &LightRagWidget::showDocuments);
    connect(m_client, &LightRagClient::fileUploaded, this, [this](const QString& path, bool ok, const QString& message) {
        m_uploadProgress->setValue(m_uploadProgress->value() + 1);
        if (!ok)
            setStatus(tr("%1: %2").arg(QFileInfo(path).fileName(), message), true);
        if (--m_uploadsPending == 0) {
            setStatus(tr("Upload finished. The server processes documents in the background; refresh to see their status."));
            m_client->listDocuments();
        }
    });
    connect(m_client, &LightRagClient::answered, this, [this](const QString& answer) {
        m_askButton->setEnabled(true);
        m_answer->setMarkdown(answer);
        setStatus(tr("Answered."));
    });
    connect(m_client, &LightRagClient::graphLabelsListed, this, [this](const QStringList& labels) {
        m_labels->clear();
        m_labels->addItem(QStringLiteral("*"));
        m_labels->addItems(labels);
    });
    connect(m_client, &LightRagClient::graphLoaded, this, &LightRagWidget::showGraph);

    connectToServer();
}

QWidget* LightRagWidget::buildDocumentsTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    auto* buttons = new QHBoxLayout;
    auto* addFiles = new QPushButton(tr("Upload files..."), page);
    auto* addFolder = new QPushButton(tr("Upload folder..."), page);
    auto* refresh = new QPushButton(tr("Refresh"), page);
    m_uploadProgress = new QProgressBar(page);
    buttons->addWidget(addFiles);
    buttons->addWidget(addFolder);
    buttons->addWidget(refresh);
    buttons->addWidget(m_uploadProgress, 1);
    layout->addLayout(buttons);

    m_documents = new QTableWidget(0, 4, page);
    m_documents->setHorizontalHeaderLabels({tr("File"), tr("Status"), tr("Chunks"), tr("Updated")});
    m_documents->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_documents->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_documents, 1);

    connect(addFiles, &QPushButton::clicked, this, [this]() {
        uploadFiles(QFileDialog::getOpenFileNames(this, tr("Documents to upload")));
    });
    connect(addFolder, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Folder to upload"));
        if (dir.isEmpty())
            return;
        QStringList files;
        const QStringList filters = {"*.txt", "*.md", "*.pdf", "*.docx", "*.pptx", "*.xlsx", "*.html", "*.csv", "*.json"};
        for (const QFileInfo& fi : QDir(dir).entryInfoList(filters, QDir::Files))
            files << fi.absoluteFilePath();
        uploadFiles(files);
    });
    connect(refresh, &QPushButton::clicked, m_client, &LightRagClient::listDocuments);
    return page;
}

QWidget* LightRagWidget::buildQueryTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    auto* row = new QHBoxLayout;
    m_mode = new QComboBox(page);
    // LightRAG retrieval modes.
    m_mode->addItem(tr("mix: graph and vector search"), "mix");
    m_mode->addItem(tr("hybrid: local and global graph"), "hybrid");
    m_mode->addItem(tr("local: entities near the question"), "local");
    m_mode->addItem(tr("global: relations across documents"), "global");
    m_mode->addItem(tr("naive: plain vector search"), "naive");
    m_askButton = new QPushButton(tr("Ask"), page);
    row->addWidget(new QLabel(tr("Mode"), page));
    row->addWidget(m_mode, 1);
    row->addWidget(m_askButton);
    layout->addLayout(row);

    m_question = new QPlainTextEdit(page);
    m_question->setPlaceholderText(tr("Question about the uploaded documents"));
    m_question->setMaximumHeight(100);
    layout->addWidget(m_question);
    m_answer = new QTextBrowser(page);
    layout->addWidget(m_answer, 1);

    connect(m_askButton, &QPushButton::clicked, this, [this]() {
        const QString q = m_question->toPlainText().trimmed();
        if (q.isEmpty())
            return;
        m_askButton->setEnabled(false);
        m_answer->clear();
        setStatus(tr("Waiting for the server..."));
        m_client->query(q, m_mode->currentData().toString());
    });
    return page;
}

QWidget* LightRagWidget::buildGraphTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    auto* row = new QHBoxLayout;
    m_labels = new QComboBox(page);
    m_labels->setEditable(true);
    m_depth = new QSpinBox(page);
    m_depth->setRange(1, 5);
    m_depth->setValue(2);
    auto* show = new QPushButton(tr("Show"), page);
    row->addWidget(new QLabel(tr("Entity"), page));
    row->addWidget(m_labels, 1);
    row->addWidget(new QLabel(tr("Depth"), page));
    row->addWidget(m_depth);
    row->addWidget(show);
    layout->addLayout(row);

    m_scene = new QGraphicsScene(page);
    m_view = new ZoomView(m_scene, page);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    layout->addWidget(m_view, 1);

    connect(show, &QPushButton::clicked, this, [this]() {
        const QString label = m_labels->currentText().trimmed();
        if (!label.isEmpty())
            m_client->loadGraph(label, m_depth->value());
    });
    return page;
}

void LightRagWidget::connectToServer()
{
    const QUrl url = QUrl::fromUserInput(m_serverEdit->text().trimmed());
    QSettings s;
    s.setValue("lightrag/server", url.toString());
    s.setValue("lightrag/apiKey", m_apiKeyEdit->text().trimmed());
    m_client->setServer(url, m_apiKeyEdit->text());
    setStatus(tr("Connecting to %1...").arg(url.toString()));
    m_client->checkHealth();
}

void LightRagWidget::uploadFiles(const QStringList& files)
{
    if (files.isEmpty())
        return;
    m_uploadsPending = files.size();
    m_uploadProgress->setRange(0, files.size());
    m_uploadProgress->setValue(0);
    setStatus(tr("Uploading %n file(s)...", nullptr, files.size()));
    for (const QString& f : files)
        m_client->uploadFile(f);
}

void LightRagWidget::showDocuments(const QJsonArray& documents)
{
    m_documents->setRowCount(0);
    for (const QJsonValue& v : documents) {
        const QJsonObject d = v.toObject();
        const int row = m_documents->rowCount();
        m_documents->insertRow(row);
        QString file = d.value(QStringLiteral("file_path")).toString();
        if (file.isEmpty())
            file = d.value(QStringLiteral("id")).toString();
        m_documents->setItem(row, 0, new QTableWidgetItem(file));
        auto* status = new QTableWidgetItem(d.value(QStringLiteral("status")).toString());
        const QString error = d.value(QStringLiteral("error")).toString();
        if (!error.isEmpty())
            status->setToolTip(error);
        m_documents->setItem(row, 1, status);
        m_documents->setItem(row, 2, new QTableWidgetItem(d.value(QStringLiteral("chunks_count")).toVariant().toString()));
        m_documents->setItem(row, 3, new QTableWidgetItem(d.value(QStringLiteral("updated_at")).toString()));
    }
}

void LightRagWidget::showGraph(const QJsonArray& nodes, const QJsonArray& edges)
{
    m_scene->clear();
    const int n = nodes.size();
    if (n == 0) {
        setStatus(tr("The graph for this entity is empty."));
        return;
    }

    // Force-directed layout (Fruchterman-Reingold), small enough to run on the UI thread.
    QHash<QString, int> index;
    QVector<QPointF> pos(n), disp(n);
    for (int i = 0; i < n; ++i) {
        index.insert(nodes.at(i).toObject().value(QStringLiteral("id")).toString(), i);
        const double angle = 2 * 3.14159265358979323846 * i / n;
        pos[i] = QPointF(300 * std::cos(angle), 300 * std::sin(angle));
    }
    QVector<QPair<int, int>> links;
    for (const QJsonValue& v : edges) {
        const QJsonObject e = v.toObject();
        const int a = index.value(e.value(QStringLiteral("source")).toString(), -1);
        const int b = index.value(e.value(QStringLiteral("target")).toString(), -1);
        if (a >= 0 && b >= 0 && a != b)
            links.append({a, b});
    }
    const double area = 600.0 * 600.0 * qMax(1.0, n / 20.0);
    const double k = std::sqrt(area / n);
    double temperature = 120;
    for (int iter = 0; iter < 200; ++iter) {
        for (int i = 0; i < n; ++i) {
            disp[i] = QPointF();
            for (int j = 0; j < n; ++j) {
                if (i == j) continue;
                QPointF d = pos[i] - pos[j];
                const double dist = qMax(0.01, std::hypot(d.x(), d.y()));
                disp[i] += d / dist * (k * k / dist);
            }
        }
        for (const auto& l : links) {
            QPointF d = pos[l.first] - pos[l.second];
            const double dist = qMax(0.01, std::hypot(d.x(), d.y()));
            const QPointF f = d / dist * (dist * dist / k);
            disp[l.first] -= f;
            disp[l.second] += f;
        }
        for (int i = 0; i < n; ++i) {
            const double len = qMax(0.01, std::hypot(disp[i].x(), disp[i].y()));
            pos[i] += disp[i] / len * qMin(len, temperature);
        }
        temperature *= 0.97;
    }

    const QPen edgePen(QColor(QStringLiteral("#94a3b8")), 1);
    for (const QJsonValue& v : edges) {
        const QJsonObject e = v.toObject();
        const int a = index.value(e.value(QStringLiteral("source")).toString(), -1);
        const int b = index.value(e.value(QStringLiteral("target")).toString(), -1);
        if (a < 0 || b < 0)
            continue;
        auto* line = m_scene->addLine(QLineF(pos[a], pos[b]), edgePen);
        const QJsonObject props = e.value(QStringLiteral("properties")).toObject();
        line->setToolTip(props.value(QStringLiteral("description")).toString());
        line->setZValue(-1);
    }
    for (int i = 0; i < n; ++i) {
        const QJsonObject node = nodes.at(i).toObject();
        const QJsonObject props = node.value(QStringLiteral("properties")).toObject();
        auto* dot = m_scene->addEllipse(-7, -7, 14, 14, QPen(QColor(QStringLiteral("#0f766e"))),
                                        QBrush(QColor(QStringLiteral("#5eead4"))));
        dot->setPos(pos[i]);
        dot->setToolTip(props.value(QStringLiteral("description")).toString());
        auto* text = m_scene->addText(nodeTitle(node));
        text->setPos(pos[i] + QPointF(9, -10));
    }
    m_view->fitInView(m_scene->itemsBoundingRect().adjusted(-40, -40, 40, 40), Qt::KeepAspectRatio);
    setStatus(tr("%1 entities, %2 relations.").arg(n).arg(edges.size()));
}

void LightRagWidget::setStatus(const QString& text, bool error)
{
    m_status->setText(error ? QStringLiteral("<span style='color:#b91c1c'>%1</span>").arg(text.toHtmlEscaped())
                            : text.toHtmlEscaped());
}
