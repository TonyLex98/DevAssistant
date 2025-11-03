#include "RunTemplateOrSequenceDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QPixmap>

RunTemplateOrSequenceDialog::RunTemplateOrSequenceDialog(const QMap<QString, PromptSequence> &seqs,
                                                         SolutionTemplateManager *templateManager,
                                                         QWidget *parent)
    : QDialog(parent), m_seqs(seqs), m_templateManager(templateManager)
{
    setWindowTitle("Select Sequence or Template");
    resize(700, 500);
    
    setupUI();
    populateSequences();
    populateTemplates();
}

void RunTemplateOrSequenceDialog::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    
    // Mode selection
    auto *modeLayout = new QHBoxLayout();
    QLabel *modeLabel = new QLabel("Mode:", this);
    QComboBox *modeCombo = new QComboBox(this);
    modeCombo->addItem("Run Individual Sequence", RunSequence);
    modeCombo->addItem("Run Template", RunTemplate);
    modeCombo->setCurrentIndex(0); // Default to sequence mode
    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(modeCombo, 1);
    layout->addLayout(modeLayout);
    
    connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &RunTemplateOrSequenceDialog::onModeChanged);
    
    // Tab widget for sequences and templates
    m_tabWidget = new QTabWidget(this);
    
    // Sequences tab
    QWidget *sequenceTab = new QWidget(this);
    auto *seqLayout = new QVBoxLayout(sequenceTab);
    
    m_sequenceList = new QListWidget(this);
    seqLayout->addWidget(new QLabel("Available Sequences:", this));
    seqLayout->addWidget(m_sequenceList, 1);
    
    m_tabWidget->addTab(sequenceTab, "Sequences");
    
    // Templates tab
    QWidget *templateTab = new QWidget(this);
    auto *tmplLayout = new QVBoxLayout(templateTab);
    
    m_templateList = new QListWidget(this);
    tmplLayout->addWidget(new QLabel("Solution Templates:", this));
    tmplLayout->addWidget(m_templateList, 1);
    
    // Template details
    auto *detailsGroupBox = new QGroupBox("Template Details", this);
    auto *detailsLayout = new QVBoxLayout(detailsGroupBox);
    
    m_templateDescriptionLabel = new QLabel(this);
    m_templateDescriptionLabel->setWordWrap(true);
    m_templateImageLabel = new QLabel(this);
    m_templateImageLabel->setMinimumSize(100, 100);
    m_templateImageLabel->setAlignment(Qt::AlignCenter);
    m_templateImageLabel->setStyleSheet("border: 1px solid gray;");
    
    auto *infoLayout = new QHBoxLayout();
    auto *textLayout = new QVBoxLayout();
    textLayout->addWidget(m_templateDescriptionLabel);
    textLayout->addStretch();
    infoLayout->addLayout(textLayout);
    infoLayout->addWidget(m_templateImageLabel);
    
    // Default sequence selection
    auto *seqLayout2 = new QHBoxLayout();
    QLabel *seqLabel = new QLabel("Default Sequence:", this);
    m_defaultSequenceCombo = new MemoryComboBox("defaultTemplateSequence", this);
    seqLayout2->addWidget(seqLabel);
    seqLayout2->addWidget(m_defaultSequenceCombo, 1);
    
    detailsLayout->addLayout(infoLayout);
    detailsLayout->addLayout(seqLayout2);
    
    tmplLayout->addWidget(detailsGroupBox);
    
    m_tabWidget->addTab(templateTab, "Templates");
    
    layout->addWidget(m_tabWidget, 1);
    
    // Buttons
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, this);
    layout->addWidget(bb);
    
    connect(bb, &QDialogButtonBox::accepted, this, &RunTemplateOrSequenceDialog::onOk);
    connect(bb, &QDialogButtonBox::rejected, this, &RunTemplateOrSequenceDialog::reject);
    connect(m_templateList, &QListWidget::currentItemChanged, 
            this, [this](QListWidgetItem *current, QListWidgetItem *previous) {
        Q_UNUSED(previous);
        if (current) {
            onTemplateSelected(current->text());
        }
    });
}

void RunTemplateOrSequenceDialog::populateSequences()
{
    m_sequenceList->clear();
    
    for (auto it = m_seqs.constBegin(); it != m_seqs.constEnd(); ++it) {
        const QString &path = it.key();
        const PromptSequence &seq = it.value();
        QString display = seq.name();
        if (!seq.description().isEmpty())
            display += " – " + seq.description();
        QListWidgetItem *item = new QListWidgetItem(display, m_sequenceList);
        item->setData(Qt::UserRole, path);
    }
}

void RunTemplateOrSequenceDialog::populateTemplates()
{
    m_templateList->clear();
    
    if (m_templateManager) {
        for (const QString& templateDir : m_templateManager->templateDirs()) {
            QString templateName = QFileInfo(templateDir).fileName();
            QListWidgetItem *item = new QListWidgetItem(templateName, m_templateList);
            item->setData(Qt::UserRole, templateDir);
        }
    }
}

void RunTemplateOrSequenceDialog::onTemplateSelected(const QString &templateName)
{
    if (!m_templateManager) {
        return;
    }
    
    // Find the template directory by name
    QString selectedTemplateDir;
    for (const QString& templateDir : m_templateManager->templateDirs()) {
        if (QFileInfo(templateDir).fileName() == templateName) {
            selectedTemplateDir = templateDir;
            break;
        }
    }
    
    if (selectedTemplateDir.isEmpty()) {
        return;
    }
    
    // Load template details
    SolutionTemplate tmpl = m_templateManager->templates().value(selectedTemplateDir);
    m_templateDescriptionLabel->setText(tmpl.description());
    
    // Load image if available
    if (!tmpl.imagePath().isEmpty()) {
        QString imagePath = tmpl.imagePath();
        QString absoluteImagePath;
        
        // If the path is relative (contains "images/"), make it absolute relative to the template directory
        if (imagePath.startsWith("images/")) {
            absoluteImagePath = selectedTemplateDir + "/" + imagePath;
        } else {
            absoluteImagePath = imagePath; // Already absolute path
        }
        
        if (QFileInfo::exists(absoluteImagePath)) {
            QPixmap pixmap(absoluteImagePath);
            if (!pixmap.isNull()) {
                m_templateImageLabel->setPixmap(pixmap.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                m_templateImageLabel->setText("Image not found");
            }
        } else {
            m_templateImageLabel->setText("Image not found");
        }
    } else {
        m_templateImageLabel->clear();
    }
    
    // Load available sequences for this template
    m_defaultSequenceCombo->clear();
    QStringList seqPaths = m_templateManager->getAvailableSequences(selectedTemplateDir);
    for (const QString& seqPath : seqPaths) {
        QFileInfo info(seqPath);
        QString name = info.fileName();
        m_defaultSequenceCombo->addItem(name, seqPath);
    }
    
    // Set the main/default sequence if it exists
    QString mainSeqPath = m_templateManager->getMainSequencePath(selectedTemplateDir);
    if (!mainSeqPath.isEmpty()) {
        int index = m_defaultSequenceCombo->findData(mainSeqPath);
        if (index >= 0) {
            m_defaultSequenceCombo->setCurrentIndex(index);
        }
    }
}

void RunTemplateOrSequenceDialog::onModeChanged(int index)
{
    m_selectedMode = static_cast<DialogMode>(index);
    m_tabWidget->setCurrentIndex(index); // Switch tab based on mode
}

void RunTemplateOrSequenceDialog::onOk()
{
    if (m_selectedMode == RunSequence) {
        // Get selected sequence
        QListWidgetItem *cur = m_sequenceList->currentItem();
        if (!cur) {
            QMessageBox::warning(this, "Selection Error", "Please select a sequence.");
            return;
        }
        m_selectedSequencePath = cur->data(Qt::UserRole).toString();
    } else { // RunTemplate
        // Get selected template
        QListWidgetItem *cur = m_templateList->currentItem();
        if (!cur) {
            QMessageBox::warning(this, "Selection Error", "Please select a template.");
            return;
        }
        m_selectedTemplatePath = cur->data(Qt::UserRole).toString();
        
        // Get selected default sequence for the template
        m_selectedDefaultSequencePath = m_defaultSequenceCombo->currentData().toString();
        if (m_selectedDefaultSequencePath.isEmpty()) {
            QMessageBox::warning(this, "Selection Error", "Please select a default sequence for the template.");
            return;
        }
    }
    
    accept();
}