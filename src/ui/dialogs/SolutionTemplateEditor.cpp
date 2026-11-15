#include "ui/dialogs/SolutionTemplateEditor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QPixmap>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTextEdit>
#include <QGridLayout>
#include <QTabWidget>
#include "core/util/Logging.h"

SolutionTemplateEditor::SolutionTemplateEditor(SolutionTemplateManager *templateManager, PromptSequenceManager *sequenceManager, QWidget *parent)
    : QWidget(parent), m_templateManager(templateManager), m_sequenceManager(sequenceManager)
{
    setupUI();
    updateAvailableSequences();
}

void SolutionTemplateEditor::setupUI()
{
    auto *layout = new QVBoxLayout(this);

    // Create tab widget to organize different sections
    m_tabWidget = new QTabWidget(this);
    
    // Create the basic info widget
    QWidget *basicInfoWidget = new QWidget(this);
    auto *basicInfoLayout = new QVBoxLayout(basicInfoWidget);

    // Template information
    auto *infoGroupBox = new QGroupBox(tr("Template Information"));
    auto *infoLayout = new QGridLayout(infoGroupBox);

    QLabel *nameLabel = new QLabel(tr("Name:"), this);
    m_nameEdit = new MemoryLineEdit("solutionTemplateName", this);
    infoLayout->addWidget(nameLabel, 0, 0);
    infoLayout->addWidget(m_nameEdit, 0, 1);

    QLabel *descLabel = new QLabel(tr("Description:"), this);
    m_descriptionEdit = new MemoryLineEdit("solutionTemplateDescription", this);
    infoLayout->addWidget(descLabel, 1, 0);
    infoLayout->addWidget(m_descriptionEdit, 1, 1);

    QLabel *imageLabel = new QLabel(tr("Icon:"), this);
    m_imagePathLabel = new QLabel(tr("No image selected"), this);
    m_selectImageBtn = new StyledButton(tr("Select Image..."), this);
    auto *imageLayout = new QHBoxLayout();
    imageLayout->addWidget(m_imagePathLabel);
    imageLayout->addWidget(m_selectImageBtn);
    infoLayout->addWidget(imageLabel, 2, 0);
    infoLayout->addLayout(imageLayout, 2, 1);

    // Image preview
    m_imageLabel = new QLabel(this);
    m_imageLabel->setMinimumSize(200, 200);
    m_imageLabel->setStyleSheet("border: 1px solid gray;");
    m_imageLabel->setAlignment(Qt::AlignCenter);
    infoLayout->addWidget(m_imageLabel, 3, 0, 1, 2);

    basicInfoLayout->addWidget(infoGroupBox);

    // Sequences in the template
    auto *sequencesGroupBox = new QGroupBox(tr("Sequences in Template"));
    auto *sequencesLayout = new QVBoxLayout(sequencesGroupBox);

    auto *topLayout = new QHBoxLayout();
    
    // Available sequences (not yet in the template)
    m_availableSequencesList = new QListWidget(this);
    m_availableSequencesList->setSelectionMode(QAbstractItemView::SingleSelection);
    auto *availableGroup = new QGroupBox(tr("Available Sequences"));
    auto *availableLayout = new QVBoxLayout(availableGroup);
    availableLayout->addWidget(m_availableSequencesList);
    topLayout->addWidget(availableGroup);

    auto *btnLayout = new QVBoxLayout();
    m_addSequenceBtn = new StyledButton(tr("-> Add to Template"), this);
    m_removeSequenceBtn = new StyledButton(tr("Remove from Template <-"), this);
    m_setMainSequenceBtn = new StyledButton(tr("Set as Main"), this);
    btnLayout->addWidget(m_addSequenceBtn);
    btnLayout->addWidget(m_removeSequenceBtn);
    btnLayout->addWidget(m_setMainSequenceBtn);
    btnLayout->addStretch();
    topLayout->addLayout(btnLayout);

    // Sequences already in the template
    m_templateSequencesList = new QListWidget(this);
    m_templateSequencesList->setSelectionMode(QAbstractItemView::SingleSelection);
    auto *templateGroup = new QGroupBox(tr("Template Sequences"));
    auto *templateLayout = new QVBoxLayout(templateGroup);
    templateLayout->addWidget(m_templateSequencesList);
    topLayout->addWidget(templateGroup);

    sequencesLayout->addLayout(topLayout);

    basicInfoLayout->addWidget(sequencesGroupBox);
    basicInfoLayout->addStretch();

    // Add basic info tab
    m_tabWidget->addTab(basicInfoWidget, tr("Basic Info"));

    // Create and configure the graph widget
    m_graphWidget = new TemplateSequenceGraphWidget(this);
    m_graphWidget->setTemplateManager(m_templateManager);
    if (m_sequenceManager) {
        m_graphWidget->setSequenceManager(m_sequenceManager);
    } else {
    }
    m_tabWidget->addTab(m_graphWidget, tr("Sequence Graph"));

    layout->addWidget(m_tabWidget);

    // Connect signals
    connect(m_addSequenceBtn, &StyledButton::clicked, this, &SolutionTemplateEditor::onAddSequence);
    connect(m_removeSequenceBtn, &StyledButton::clicked, this, &SolutionTemplateEditor::onRemoveSequence);
    connect(m_setMainSequenceBtn, &StyledButton::clicked, this, &SolutionTemplateEditor::onSetMainSequence);
    connect(m_selectImageBtn, &StyledButton::clicked, this, &SolutionTemplateEditor::onSelectImage);
    connect(m_templateSequencesList, &QListWidget::currentItemChanged, this, &SolutionTemplateEditor::onSequenceSelectionChanged);
    connect(m_availableSequencesList, &QListWidget::currentItemChanged, this, &SolutionTemplateEditor::onAvailableSequenceSelectionChanged);
    connect(m_graphWidget, &TemplateSequenceGraphWidget::templateModified, this, &SolutionTemplateEditor::onTemplateModified);
}

void SolutionTemplateEditor::setTemplate(const SolutionTemplate &templateValue)
{
    m_currentTemplate = templateValue;

    // Update UI elements
    // Block signals temporarily to prevent auto-saving during initial setup
    m_nameEdit->blockSignals(true);
    m_nameEdit->setText(m_currentTemplate.name());
    m_nameEdit->blockSignals(false);
    m_descriptionEdit->setText(m_currentTemplate.description());

    // Load image if exists
    if (!m_currentTemplate.imagePath().isEmpty()) {
        QString imagePath = m_currentTemplate.imagePath();
        
        // If the path is relative (contains "images/"), make it absolute relative to the template directory
        QString absoluteImagePath;
        if (imagePath.startsWith("images/")) {
            // This is a relative path, we need to construct the full path
            // Since we don't have the template directory path here directly, we reconstruct it from the template name
            QString templateDir = m_templateManager->root() + "/" + m_currentTemplate.name();
            absoluteImagePath = templateDir + "/" + imagePath;
        } else {
            absoluteImagePath = imagePath; // Already absolute path
        }
        
        if (QFileInfo::exists(absoluteImagePath)) {
            QPixmap pixmap(absoluteImagePath);
            if (!pixmap.isNull()) {
                m_imageLabel->setPixmap(pixmap.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                m_imagePathLabel->setText(QFileInfo(absoluteImagePath).fileName());
                m_imagePath = imagePath; // Keep the original relative path
            } else {
                m_imageLabel->clear();
                m_imagePathLabel->setText(tr("Image not found"));
                m_imagePath.clear();
            }
        } else {
            m_imageLabel->clear();
            m_imagePathLabel->setText(tr("Image not found"));
            m_imagePath.clear();
        }
    } else {
        m_imageLabel->clear();
        m_imagePathLabel->setText(tr("No image selected"));
        m_imagePath.clear();
    }

    // Populate template sequences list
    m_templateSequencesList->clear();
    for (const QString &seqPath : m_currentTemplate.sequences()) {
        QFileInfo info(seqPath);
        QString displayName = info.fileName();
        auto *item = new QListWidgetItem(displayName, m_templateSequencesList);
        item->setData(Qt::UserRole, seqPath);
        
        // Mark main sequence
        if (seqPath == m_currentTemplate.mainSequencePath()) {
            item->setCheckState(Qt::Checked);
        } else {
            item->setCheckState(Qt::Unchecked);
        }
    }
    
    updateAvailableSequences();
    
    // Update the graph widget with the current template
    if (m_graphWidget) {
        m_graphWidget->setTemplate(m_currentTemplate);
    } else {
    }
}

SolutionTemplate SolutionTemplateEditor::templateValue() const
{
    SolutionTemplate tmpl = m_currentTemplate;
    tmpl.setName(m_nameEdit->text());
    tmpl.setDescription(m_descriptionEdit->text());
    tmpl.setImagePath(m_imagePath);
    
    // Collect sequences from the list
    QStringList seqPaths;
    for (int i = 0; i < m_templateSequencesList->count(); ++i) {
        QListWidgetItem *item = m_templateSequencesList->item(i);
        QString path = item->data(Qt::UserRole).toString();
        seqPaths << path;
        
        // Check if this is the main sequence
        if (item->checkState() == Qt::Checked) {
            tmpl.setMainSequencePath(path);
        }
    }
    tmpl.clearSequences();
    for (const QString &path : seqPaths) {
        tmpl.addSequence(path);
    }
    
    return tmpl;
}

void SolutionTemplateEditor::updateAvailableSequences()
{
    if (!m_templateManager || !m_sequenceManager) {
        return;
    }

    m_availableSequencesList->clear();
    
    const QMap<QString, PromptSequence>& allSequences = m_sequenceManager->sequences();
    
    // Get sequences that are already in the current template
    QStringList templateSequences;
    for (const QString &seqPath : m_currentTemplate.sequences()) {
        QFileInfo info(seqPath);
        templateSequences << info.fileName().replace(".seq.json", "");
    }
    
    // Add sequences that are not already in the template
    for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
        const QString &seqPath = it.key(); // Key is the file path
        const PromptSequence &seq = it.value();
        if (!templateSequences.contains(seq.name())) {
            auto *item = new QListWidgetItem(seq.name(), m_availableSequencesList);
            item->setData(Qt::UserRole, seqPath); // Store the full path
        }
    }
}

void SolutionTemplateEditor::onAddSequence()
{
    QListWidgetItem *item = m_availableSequencesList->currentItem();
    if (!item) {
        QMessageBox::information(this, tr("No Selection"), tr("Please select a sequence to add."));
        return;
    }

    QString seqPath = item->data(Qt::UserRole).toString();
    if (seqPath.isEmpty()) return;

    // Get the template directory path
    QString templateDir = m_templateManager->root() + "/" + m_currentTemplate.name();
    QDir dir;
    if (!dir.mkpath(templateDir)) {
        QMessageBox::warning(this, tr("Create Directory"), 
                           tr("Could not create template directory: %1").arg(templateDir));
        return;
    }

    QString seqFileName = QFileInfo(seqPath).fileName();
    QString targetPath = templateDir + "/" + seqFileName;

    // Copy sequence file to template directory
    if (QFile::exists(targetPath)) {
        QMessageBox::warning(this, tr("Add Sequence"), 
                           tr("Sequence already exists in template directory: %1").arg(targetPath));
        return;
    }

    if (!QFile::copy(seqPath, targetPath)) {
        QMessageBox::warning(this, tr("Add Sequence"), 
                           tr("Failed to copy sequence to template directory: %1").arg(targetPath));
        return;
    }

    // Add to template
    m_currentTemplate.addSequence(targetPath);

    // Update UI
    auto *newItem = new QListWidgetItem(QFileInfo(targetPath).fileName(), m_templateSequencesList);
    newItem->setData(Qt::UserRole, targetPath);
    newItem->setCheckState(Qt::Unchecked);
    
    // Remove from available list
    int row = m_availableSequencesList->row(item);
    m_availableSequencesList->takeItem(row);
    delete item;
    
    // Update the graph widget
    if (m_graphWidget) {
        m_graphWidget->setTemplate(m_currentTemplate);
    }
    
    emit templateModified();
}

void SolutionTemplateEditor::onRemoveSequence()
{
    QListWidgetItem *item = m_templateSequencesList->currentItem();
    if (!item) {
        QMessageBox::information(this, tr("No Selection"), tr("Please select a sequence to remove."));
        return;
    }

    QString seqPath = item->data(Qt::UserRole).toString();
    if (seqPath.isEmpty()) return;

    // If this is the main sequence, clear main sequence path
    if (seqPath == m_currentTemplate.mainSequencePath()) {
        m_currentTemplate.setMainSequencePath("");
    }

    // Remove from template
    int idx = -1;
    for (int i = 0; i < m_currentTemplate.sequences().size(); ++i) {
        if (m_currentTemplate.sequences().at(i) == seqPath) {
            idx = i;
            break;
        }
    }
    if (idx >= 0) {
        m_currentTemplate.removeSequence(idx);
    }
    
    // Remove from UI
    int row = m_templateSequencesList->row(item);
    m_templateSequencesList->takeItem(row);
    delete item;
    
    // Add back to available list
    QFileInfo info(seqPath);
    QString name = info.fileName();
    auto *availableItem = new QListWidgetItem(name, m_availableSequencesList);
    availableItem->setData(Qt::UserRole, seqPath);
    m_seqPathByName.insert(name, seqPath);
    
    // Update the graph widget
    if (m_graphWidget) {
        m_graphWidget->setTemplate(m_currentTemplate);
    }
    
    emit templateModified();
}

void SolutionTemplateEditor::onSetMainSequence()
{
    QListWidgetItem *item = m_templateSequencesList->currentItem();
    if (!item) {
        QMessageBox::information(this, tr("No Selection"), tr("Please select a sequence to set as main."));
        return;
    }

    QString seqPath = item->data(Qt::UserRole).toString();
    if (seqPath.isEmpty()) return;

    // Uncheck all others
    for (int i = 0; i < m_templateSequencesList->count(); ++i) {
        QListWidgetItem *otherItem = m_templateSequencesList->item(i);
        otherItem->setCheckState(Qt::Unchecked);
    }

    // Check this item and set as main
    item->setCheckState(Qt::Checked);
    m_currentTemplate.setMainSequencePath(seqPath);
    
    // Update the graph widget
    if (m_graphWidget) {
        m_graphWidget->setTemplate(m_currentTemplate);
    }
    
    emit templateModified();
}

void SolutionTemplateEditor::onSelectImage()
{
    QString imagePath = QFileDialog::getOpenFileName(this, tr("Select Image"),
                                                   QDir::homePath(),
                                                   tr("Image Files (*.png *.jpg *.jpeg *.bmp *.svg *.gif)"));
    if (imagePath.isEmpty()) return;

    QPixmap pixmap(imagePath);
    if (pixmap.isNull()) {
        QMessageBox::warning(this, tr("Select Image"), tr("Could not load image"));
        return;
    }

    // Get the template directory path
    QString templateDir = m_templateManager->root() + "/" + m_currentTemplate.name();
    if (templateDir.isEmpty() || templateDir == "/") {
        // If we don't have a template name yet, use the current template's directory path from the manager
        // We need to get the current template path differently
        // For new templates, we'll use the name from the edit field
        QString templateName = m_nameEdit->text().trimmed();
        if (!templateName.isEmpty()) {
            templateDir = m_templateManager->root() + "/" + templateName;
        } else {
            QMessageBox::warning(this, tr("Select Image"), tr("Template name is not set. Please set a template name first."));
            return;
        }
    }
    
    QDir dir;
    QString imagesDir = templateDir + "/images";
    if (!dir.mkpath(imagesDir)) {
        QMessageBox::warning(this, tr("Select Image"), 
                           tr("Could not create images directory: %1").arg(imagesDir));
        return;
    }

    QString imageFileName = QFileInfo(imagePath).fileName();
    QString targetImagePath = imagesDir + "/" + imageFileName;

    // If file with same name already exists, add a number to avoid overwriting
    int counter = 1;
    QString baseName = QFileInfo(imageFileName).baseName();
    QString suffix = QFileInfo(imageFileName).suffix();
    while (QFile::exists(targetImagePath)) {
        QString newFileName = QString("%1_%2.%3").arg(baseName).arg(counter).arg(suffix);
        targetImagePath = imagesDir + "/" + newFileName;
        counter++;
    }

    // Copy the image to the template's images directory
    if (!QFile::copy(imagePath, targetImagePath)) {
        QMessageBox::warning(this, tr("Select Image"), 
                           tr("Failed to copy image to template directory: %1").arg(targetImagePath));
        return;
    }

    m_imageLabel->setPixmap(pixmap.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_imagePathLabel->setText(QFileInfo(targetImagePath).fileName());
    
    // Store relative path from template directory for portability
    m_imagePath = "images/" + QFileInfo(targetImagePath).fileName();
    
    emit templateModified();
}

void SolutionTemplateEditor::onSequenceSelectionChanged()
{
    // Update the main sequence button state based on selection
    QListWidgetItem *item = m_templateSequencesList->currentItem();
    m_setMainSequenceBtn->setEnabled(item != nullptr);
}

void SolutionTemplateEditor::onAvailableSequenceSelectionChanged()
{
    // Update the add sequence button state based on selection
    QListWidgetItem *item = m_availableSequencesList->currentItem();
    m_addSequenceBtn->setEnabled(item != nullptr);
}

void SolutionTemplateEditor::onTemplateModified()
{
    // Update the template in the editor when the graph widget modifies it
    // This method will update the internal template and ensure sync between components
    if (m_graphWidget) {
        // The graph widget manages its own template state, but we need to update our internal state
        // Since the graph widget should be reflecting the same template, we can sync back if needed
        
        // For now, just emit that the template was modified
        emit templateModified();
    }
}
