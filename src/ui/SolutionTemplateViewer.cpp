#include "SolutionTemplateViewer.h"
#include <QFormLayout>
#include <QGroupBox>
#include <QDialog>
#include <QDialogButtonBox>
#include "SolutionTemplateEditor.h"

SolutionTemplateViewer::SolutionTemplateViewer(SolutionTemplateManager* templateManager, QWidget *parent)
    : QWidget(parent), m_templateManager(templateManager)
{
    setupUI();
    updateTemplateList();
    
    // Connect signals
    connect(m_templateList, &QListWidget::currentRowChanged, this, &SolutionTemplateViewer::onTemplateSelected);
    connect(m_editTemplateBtn, &StyledButton::clicked, this, &SolutionTemplateViewer::onEditTemplate);
    connect(m_deleteTemplateBtn, &StyledButton::clicked, this, &SolutionTemplateViewer::onDeleteTemplate);
    connect(m_addTemplateBtn, &StyledButton::clicked, this, &SolutionTemplateViewer::onAddTemplate);
    connect(m_exportTemplateBtn, &StyledButton::clicked, this, &SolutionTemplateViewer::onExportTemplate);
    connect(m_importSequenceBtn, &StyledButton::clicked, this, &SolutionTemplateViewer::onImportSequenceAsTemplate);
    connect(m_runTemplateBtn, &StyledButton::clicked, this, [this]() {
        if (!m_currentTemplateDir.isEmpty()) {
            // Emit signal to run the template with the selected sequence
            QString selectedSeq = m_defaultSequenceCombo->currentData().toString();
            if (!selectedSeq.isEmpty()) {
                // Here we would run the selected sequence, but for now just show a message
                QMessageBox::information(this, "Run Template", 
                    QString("Would run template with sequence: %1").arg(selectedSeq));
            } else {
                QMessageBox::warning(this, "No Sequence", "No sequence selected for this template.");
            }
        }
    });
}

void SolutionTemplateViewer::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    
    // Template list
    auto* listGroupBox = new QGroupBox(tr("Solution Templates"));
    auto* listLayout = new QVBoxLayout(listGroupBox);
    
    m_templateList = new QListWidget(this);
    listLayout->addWidget(m_templateList);
    
    auto* listButtonsLayout = new QHBoxLayout();
    m_addTemplateBtn = new StyledButton(tr("Add Template"), this);
    m_editTemplateBtn = new StyledButton(tr("Edit Template"), this);
    m_deleteTemplateBtn = new StyledButton(tr("Delete Template"), this);
    m_exportTemplateBtn = new StyledButton(tr("Export Template"), this);
    m_importSequenceBtn = new StyledButton(tr("Import Sequence as Template"), this);
    listButtonsLayout->addWidget(m_addTemplateBtn);
    listButtonsLayout->addWidget(m_editTemplateBtn);
    listButtonsLayout->addWidget(m_deleteTemplateBtn);
    listButtonsLayout->addWidget(m_exportTemplateBtn);
    listButtonsLayout->addWidget(m_importSequenceBtn);
    listLayout->addLayout(listButtonsLayout);
    
    layout->addWidget(listGroupBox);
    
    // Template details
    auto* detailsGroupBox = new QGroupBox(tr("Template Details"));
    auto* detailsLayout = new QVBoxLayout(detailsGroupBox);
    
    m_templateNameLabel = new QLabel(tr("No template selected"), this);
    m_templateNameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    
    m_templateDescriptionLabel = new QLabel(this);
    m_templateDescriptionLabel->setWordWrap(true);
    m_templateDescriptionLabel->setStyleSheet("margin-top: 5px;");
    
    // Image label
    m_templateImageLabel = new QLabel(this);
    m_templateImageLabel->setMinimumSize(200, 200);
    m_templateImageLabel->setAlignment(Qt::AlignCenter);
    m_templateImageLabel->setStyleSheet("border: 1px solid gray;");
    
    auto* infoLayout = new QHBoxLayout();
    auto* textLayout = new QVBoxLayout();
    textLayout->addWidget(m_templateNameLabel);
    textLayout->addWidget(m_templateDescriptionLabel);
    textLayout->addStretch();
    infoLayout->addLayout(textLayout);
    infoLayout->addWidget(m_templateImageLabel);
    
    // Default sequence selection
    auto* seqLayout = new QHBoxLayout();
    auto* seqLabel = new QLabel(tr("Default Sequence:"), this);
    qDebug() << "[SolutionTemplateViewer] Creating MemoryComboBox for defaultTemplateSequence";
    m_defaultSequenceCombo = new MemoryComboBox("defaultTemplateSequence", this);
    qDebug() << "[SolutionTemplateViewer] Created MemoryComboBox for defaultTemplateSequence";
    m_runTemplateBtn = new StyledButton(tr("Run Template"), this);
    seqLayout->addWidget(seqLabel);
    seqLayout->addWidget(m_defaultSequenceCombo, 1);
    seqLayout->addWidget(m_runTemplateBtn);
    
    detailsLayout->addLayout(infoLayout);
    detailsLayout->addLayout(seqLayout);
    
    layout->addWidget(detailsGroupBox);
}

void SolutionTemplateViewer::refreshTemplates()
{
    m_templateManager->refresh();
    updateTemplateList();
}

void SolutionTemplateViewer::onTemplateSelected(int index)
{
    if (index < 0 || index >= m_templateManager->templateDirs().size()) {
        m_currentTemplateDir.clear();
        loadTemplateInfo(QString());
        return;
    }
    
    QString templateDir = m_templateManager->templateDirs().at(index);
    m_currentTemplateDir = templateDir;
    loadTemplateInfo(templateDir);
}

void SolutionTemplateViewer::onEditTemplate()
{
    qDebug() << "[SolutionTemplateViewer] onEditTemplate called";
    if (m_currentTemplateDir.isEmpty()) {
        qDebug() << "[SolutionTemplateViewer] No template selected, showing info message";
        QMessageBox::information(this, "Edit Template", "Please select a template to edit.");
        return;
    }
    
    qDebug() << "[SolutionTemplateViewer] Current template dir:" << m_currentTemplateDir;
    
    // Get the template to edit
    SolutionTemplate tmpl = m_templateManager->templates().value(m_currentTemplateDir);
    qDebug() << "[SolutionTemplateViewer] Got template, name:" << tmpl.name();
    
    // Create the editor dialog
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Edit Solution Template"));
    dialog->setMinimumSize(800, 600);
    
    auto *layout = new QVBoxLayout(dialog);
    
    // Get sequence manager, with null check for safety
    PromptSequenceManager *seqMgr = m_templateManager->sequenceManager();
    qDebug() << "[SolutionTemplateViewer] Sequence manager pointer:" << seqMgr;
    if (!seqMgr) {
        qDebug() << "[SolutionTemplateViewer] Sequence manager is null, showing warning";
        QMessageBox::warning(this, "Edit Template", "Sequence manager is not available. Template editing requires a valid sequence manager.");
        dialog->deleteLater();
        return;
    }
    
    qDebug() << "[SolutionTemplateViewer] Creating SolutionTemplateEditor with template manager:" << m_templateManager << "and sequence manager:" << seqMgr;
    
    // Create the editor widget
    SolutionTemplateEditor *editor = new SolutionTemplateEditor(m_templateManager, seqMgr, dialog);
    qDebug() << "[SolutionTemplateViewer] Created editor, setting template";
    editor->setTemplate(tmpl);
    qDebug() << "[SolutionTemplateViewer] Template set, continuing with UI setup";
    
    layout->addWidget(editor);
    
    // Add dialog buttons
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    layout->addWidget(buttonBox);
    
    // Connect buttons
    connect(buttonBox, &QDialogButtonBox::accepted, dialog, [this, dialog, editor, tmpl]() {
        SolutionTemplate updatedTemplate = editor->templateValue();
        if (m_templateManager->save(updatedTemplate, m_currentTemplateDir)) {
            // Update the template list to reflect changes
            updateTemplateList();
            loadTemplateInfo(m_currentTemplateDir);
            dialog->accept();
        } else {
            QMessageBox::warning(dialog, "Save Template", "Failed to save template.");
        }
    });
    connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    
    qDebug() << "[SolutionTemplateViewer] Showing dialog";
    // Show the dialog
    dialog->exec();
    qDebug() << "[SolutionTemplateViewer] Dialog closed, cleaning up";
    dialog->deleteLater();
    qDebug() << "[SolutionTemplateViewer] Dialog cleanup complete";
}

void SolutionTemplateViewer::onDeleteTemplate()
{
    if (m_currentTemplateDir.isEmpty()) {
        QMessageBox::information(this, "Delete Template", "Please select a template to delete.");
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Template", 
        QString("Are you sure you want to delete the template '%1'?").arg(
            QFileInfo(m_currentTemplateDir).fileName()),
        QMessageBox::Yes | QMessageBox::No);
        
    if (reply == QMessageBox::Yes) {
        if (m_templateManager->remove(m_currentTemplateDir)) {
            refreshTemplates();
            m_currentTemplateDir.clear();
        } else {
            QMessageBox::warning(this, "Delete Template", "Failed to delete template.");
        }
    }
}

void SolutionTemplateViewer::onAddTemplate()
{
    bool ok;
    QString name = QInputDialog::getText(this, "New Template", "Template name:", QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    
    // Create new template directory
    QString templateDir = QDir(m_templateManager->root()).absoluteFilePath(name.trimmed());
    if (QDir().exists(templateDir)) {
        QMessageBox::warning(this, "New Template", "Template with this name already exists.");
        return;
    }
    
    // Create template
    SolutionTemplate tmpl(name.trimmed());
    tmpl.setDescription("New solution template");
    
    if (m_templateManager->save(tmpl, templateDir)) {
        refreshTemplates();
        // Select the new template
        int index = m_templateManager->templateDirs().indexOf(templateDir);
        if (index >= 0) {
            m_templateList->setCurrentRow(index);
        }
    } else {
        QMessageBox::warning(this, "New Template", "Failed to create template.");
    }
}

void SolutionTemplateViewer::updateTemplateList()
{
    m_templateList->clear();
    
    for (const QString& templateDir : m_templateManager->templateDirs()) {
        QString templateName = QFileInfo(templateDir).fileName();
        m_templateList->addItem(templateName);
    }
}

void SolutionTemplateViewer::loadTemplateInfo(const QString& templateDir)
{
    if (templateDir.isEmpty()) {
        m_templateNameLabel->setText("No template selected");
        m_templateDescriptionLabel->clear();
        m_templateImageLabel->clear();
        m_defaultSequenceCombo->clear();
        return;
    }
    
    // Load template data
    SolutionTemplate tmpl = m_templateManager->templates().value(templateDir);
    
    m_templateNameLabel->setText(tmpl.name());
    m_templateDescriptionLabel->setText(tmpl.description());
    
    // Load image if available
    if (!tmpl.imagePath().isEmpty()) {
        QString imagePath = tmpl.imagePath();
        QString absoluteImagePath;
        
        // If the path is relative (contains "images/"), make it absolute relative to the template directory
        if (imagePath.startsWith("images/")) {
            absoluteImagePath = templateDir + "/" + imagePath;
        } else {
            absoluteImagePath = imagePath; // Already absolute path
        }
        
        if (QFileInfo::exists(absoluteImagePath)) {
            QPixmap pixmap(absoluteImagePath);
            if (!pixmap.isNull()) {
                m_templateImageLabel->setPixmap(pixmap.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
    QStringList seqPaths = m_templateManager->getAvailableSequences(templateDir);
    for (const QString& seqPath : seqPaths) {
        QFileInfo info(seqPath);
        QString name = info.fileName();
        m_defaultSequenceCombo->addItem(name, seqPath);
    }
    
    // Set the main/default sequence if it exists
    QString mainSeqPath = m_templateManager->getMainSequencePath(templateDir);
    if (!mainSeqPath.isEmpty()) {
        int index = m_defaultSequenceCombo->findData(mainSeqPath);
        if (index >= 0) {
            m_defaultSequenceCombo->setCurrentIndex(index);
        }
    }
}

void SolutionTemplateViewer::onExportTemplate()
{
    if (m_currentTemplateDir.isEmpty()) {
        QMessageBox::information(this, "Export Template", "Please select a template to export.");
        return;
    }
    
    QString templateName = QFileInfo(m_currentTemplateDir).fileName();
    QString defaultFileName = templateName + ".zip";
    QString exportPath = QFileDialog::getSaveFileName(this, tr("Export Template"),
        QDir::homePath() + "/" + defaultFileName,
        tr("ZIP files (*.zip);;All files (*)"));
    
    if (exportPath.isEmpty()) {
        return; // User cancelled
    }
    
    if (m_templateManager->exportTemplate(m_currentTemplateDir, exportPath)) {
        QMessageBox::information(this, "Export Successful", 
            QString("Template '%1' exported successfully to:\n%2").arg(templateName, exportPath));
    } else {
        QMessageBox::warning(this, "Export Failed", 
            "Failed to export template. Check permissions and try again.");
    }
}

void SolutionTemplateViewer::onImportSequenceAsTemplate()
{
    // First, let user select a sequence file
    QString sequencePath = QFileDialog::getOpenFileName(this, tr("Select Sequence to Import as Template"),
        QDir(m_templateManager->root()).absolutePath() + "/../PromptSequences", // Default to sequences directory
        tr("Sequence files (*.seq.json);;All files (*)"));
    
    if (sequencePath.isEmpty()) {
        return; // User cancelled
    }
    
    // Ask for template name and description
    bool ok;
    QString templateName = QInputDialog::getText(this, "Template Name", "Enter template name:", 
        QLineEdit::Normal, QFileInfo(sequencePath).baseName(), &ok);
    if (!ok || templateName.trimmed().isEmpty()) {
        return;
    }
    
    QString description = QInputDialog::getMultiLineText(this, "Template Description", 
        "Enter template description (optional):", QString(), &ok);
    if (!ok) { // User cancelled, allow empty description
        description = QString();
    }
    
    // Create the template directory
    QString templateDir = QDir(m_templateManager->root()).absoluteFilePath(templateName.trimmed());
    if (QDir().exists(templateDir)) {
        QMessageBox::warning(this, "Import Failed", 
            QString("Template with name '%1' already exists. Please choose a different name.").arg(templateName));
        return;
    }
    
    // Export the sequence as a template
    if (m_templateManager->exportSequenceAsTemplate(sequencePath, templateDir, templateName.trimmed(), description)) {
        refreshTemplates();
        QMessageBox::information(this, "Import Successful", 
            QString("Sequence '%1' imported as template '%2' successfully!").arg(
                QFileInfo(sequencePath).fileName(), templateName));
    } else {
        QMessageBox::warning(this, "Import Failed", 
            "Failed to import sequence as template. Check permissions and try again.");
    }
}