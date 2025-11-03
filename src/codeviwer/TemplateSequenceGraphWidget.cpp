#include "TemplateSequenceGraphWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QGroupBox>
#include <QGridLayout>
#include <QJsonObject>
#include <QJsonArray>

TemplateSequenceGraphWidget::TemplateSequenceGraphWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void TemplateSequenceGraphWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    
    // Create tab widget
    m_tabs = new QTabWidget(this);
    
    // Template Info Tab
    m_templateInfoTab = new QWidget(this);
    auto *infoLayout = new QVBoxLayout(m_templateInfoTab);
    
    auto *infoGroupBox = new QGroupBox(tr("Template Information"));
    auto *infoGrid = new QGridLayout(infoGroupBox);
    
    QLabel *nameLabel = new QLabel(tr("Name:"), this);
    m_nameEdit = new QLineEdit(this);
    infoGrid->addWidget(nameLabel, 0, 0);
    infoGrid->addWidget(m_nameEdit, 0, 1);
    
    QLabel *descLabel = new QLabel(tr("Description:"), this);
    m_descriptionEdit = new QLineEdit(this);
    infoGrid->addWidget(descLabel, 1, 0);
    infoGrid->addWidget(m_descriptionEdit, 1, 1);
    
    QLabel *imageLabel = new QLabel(tr("Icon:"), this);
    m_imageLabel = new QLabel(tr("No image selected"), this);
    m_imageLabel->setMinimumSize(200, 200);
    m_imageLabel->setStyleSheet("border: 1px solid gray;");
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_selectImageBtn = new QPushButton(tr("Select Image..."), this);
    auto *imageLayout = new QHBoxLayout();
    imageLayout->addWidget(m_imageLabel);
    imageLayout->addWidget(m_selectImageBtn);
    infoGrid->addWidget(imageLabel, 2, 0);
    infoGrid->addLayout(imageLayout, 2, 1);
    
    infoLayout->addWidget(infoGroupBox);
    infoLayout->addStretch();
    
    // Sequences Tab
    m_sequencesTab = new QWidget(this);
    m_sequencesLayout = new QHBoxLayout(m_sequencesTab);
    
    // Available sequences on the left
    m_availableSequencesLayout = new QVBoxLayout();
    m_availableSequencesList = new QListWidget(this);
    m_availableSequencesList->setSelectionMode(QAbstractItemView::SingleSelection);
    auto *availableGroup = new QGroupBox(tr("Available Sequences"));
    auto *availableLayout = new QVBoxLayout(availableGroup);
    availableLayout->addWidget(m_availableSequencesList);
    m_availableSequencesLayout->addWidget(availableGroup);
    
    // Template sequences in the middle
    m_templateSequencesLayout = new QVBoxLayout();
    m_templateSequencesList = new QListWidget(this);
    m_templateSequencesList->setSelectionMode(QAbstractItemView::SingleSelection);
    auto *templateGroup = new QGroupBox(tr("Template Sequences"));
    auto *templateLayout = new QVBoxLayout(templateGroup);
    templateLayout->addWidget(m_templateSequencesList);
    m_templateSequencesLayout->addWidget(templateGroup);
    
    // Buttons in the center
    m_buttonsLayout = new QVBoxLayout();
    m_addSequenceBtn = new QPushButton(tr("→ Add to Template"), this);
    m_removeSequenceBtn = new QPushButton(tr("Remove from Template ←"), this);
    m_setMainSequenceBtn = new QPushButton(tr("Set as Main"), this);
    m_buttonsLayout->addWidget(m_addSequenceBtn);
    m_buttonsLayout->addWidget(m_removeSequenceBtn);
    m_buttonsLayout->addWidget(m_setMainSequenceBtn);
    m_buttonsLayout->addStretch();
    
    m_sequencesLayout->addLayout(m_availableSequencesLayout);
    m_sequencesLayout->addLayout(m_buttonsLayout);
    m_sequencesLayout->addLayout(m_templateSequencesLayout);
    
    // Graph Tab
    m_graphTab = new QWidget(this);
    m_graphLayout = new QVBoxLayout(m_graphTab);
    
    m_graphView = new SequenceRelationView(this);
    m_refreshGraphBtn = new QPushButton(tr("Refresh Graph"), this);
    
    m_graphLayout->addWidget(m_graphView);
    m_graphLayout->addWidget(m_refreshGraphBtn);
    
    // Add tabs
    m_tabs->addTab(m_templateInfoTab, tr("Template Info"));
    m_tabs->addTab(m_sequencesTab, tr("Sequences"));
    m_tabs->addTab(m_graphTab, tr("Sequence Graph"));
    
    m_mainLayout->addWidget(m_tabs);
    
    // Connect signals
    connect(m_addSequenceBtn, &QPushButton::clicked, this, &TemplateSequenceGraphWidget::onAddSequenceToTemplate);
    connect(m_removeSequenceBtn, &QPushButton::clicked, this, &TemplateSequenceGraphWidget::onRemoveSequenceFromTemplate);
    connect(m_setMainSequenceBtn, &QPushButton::clicked, this, &TemplateSequenceGraphWidget::onSetMainSequence);
    connect(m_selectImageBtn, &QPushButton::clicked, this, &TemplateSequenceGraphWidget::onSelectImage);
    connect(m_templateSequencesList, &QListWidget::currentItemChanged, this, &TemplateSequenceGraphWidget::onSequenceInTemplateSelected);
    connect(m_availableSequencesList, &QListWidget::currentItemChanged, this, &TemplateSequenceGraphWidget::onExternalSequenceSelected);
    connect(m_refreshGraphBtn, &QPushButton::clicked, this, &TemplateSequenceGraphWidget::refreshGraph);
    connect(m_graphView, &SequenceRelationView::sequenceSelected, this, &TemplateSequenceGraphWidget::onGraphSequenceSelected);
}

void TemplateSequenceGraphWidget::setTemplateManager(SolutionTemplateManager *templateManager)
{
    m_templateManager = templateManager;
}

void TemplateSequenceGraphWidget::setSequenceManager(PromptSequenceManager *seqManager)
{
    qDebug() << "[TemplateSequenceGraphWidget] setSequenceManager called with seqManager:" << seqManager;
    m_sequenceManager = seqManager;
    if (m_graphView) {
        qDebug() << "[TemplateSequenceGraphWidget] Setting sequence manager on graph view";
        m_graphView->setSequenceManager(seqManager);
        qDebug() << "[TemplateSequenceGraphWidget] Sequence manager set on graph view";
    } else {
        qDebug() << "[TemplateSequenceGraphWidget] Warning: m_graphView is null, cannot set sequence manager";
    }
}

void TemplateSequenceGraphWidget::setTemplate(const SolutionTemplate &templateValue)
{
    m_currentTemplate = templateValue;
    
    // Update UI elements with template data
    m_nameEdit->setText(m_currentTemplate.name());
    m_descriptionEdit->setText(m_currentTemplate.description());
    
    // Load image if exists
    if (!m_currentTemplate.imagePath().isEmpty()) {
        QString imagePath = m_currentTemplate.imagePath();
        QString absoluteImagePath;
        
        // If the path is relative (contains "images/"), make it absolute relative to the template directory
        if (imagePath.startsWith("images/")) {
            // Get the template directory path to construct the absolute path
            QString templateDir = m_templateManager->root() + "/" + m_currentTemplate.name();
            absoluteImagePath = templateDir + "/" + imagePath;
        } else {
            absoluteImagePath = imagePath; // Already absolute path
        }
        
        if (QFileInfo::exists(absoluteImagePath)) {
            QPixmap pixmap(absoluteImagePath);
            if (!pixmap.isNull()) {
                m_imageLabel->setPixmap(pixmap.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                m_imagePath = absoluteImagePath; // Store absolute path for this view
            } else {
                m_imageLabel->clear();
                m_imageLabel->setText(tr("Image not found"));
                m_imagePath.clear();
            }
        } else {
            m_imageLabel->clear();
            m_imageLabel->setText(tr("Image not found"));
            m_imagePath.clear();
        }
    } else {
        m_imageLabel->clear();
        m_imageLabel->setText(tr("No image selected"));
        m_imagePath.clear();
    }
    
    updateTemplateSequencesList();
    updateAvailableSequences();
    buildGraphForTemplate();
}

void TemplateSequenceGraphWidget::updateTemplateSequencesList()
{
    m_templateSequencesList->clear();
    
    for (const QString &seqPath : m_currentTemplate.sequences()) {
        QFileInfo info(seqPath);
        QString displayName = info.fileName().replace(".seq.json", "");
        auto *item = new QListWidgetItem(displayName, m_templateSequencesList);
        item->setData(Qt::UserRole, seqPath);
        
        // Mark main sequence
        if (seqPath == m_currentTemplate.mainSequencePath()) {
            item->setCheckState(Qt::Checked);
        } else {
            item->setCheckState(Qt::Unchecked);
        }
    }
}

void TemplateSequenceGraphWidget::updateAvailableSequences()
{
    m_availableSequencesList->clear();
    
    if (!m_sequenceManager) return;
    
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

void TemplateSequenceGraphWidget::buildGraphForTemplate()
{
    if (!m_templateManager) return;
    
    // Find the directory path for this template
    QString templateDir = m_templateManager->root() + "/" + m_currentTemplate.name();
    if (QDir(templateDir).exists()) {
        m_graphView->rebuildGraphForTemplate(templateDir);
    }
}

void TemplateSequenceGraphWidget::refreshGraph()
{
    buildGraphForTemplate();
}

void TemplateSequenceGraphWidget::onAddSequenceToTemplate()
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

    // Copy sequence file to template directory
    QString seqFileName = QFileInfo(seqPath).fileName();
    QString targetPath = templateDir + "/" + seqFileName;

    if (QFile::exists(targetPath)) {
        // Check if it's already in the template
        if (!m_currentTemplate.sequences().contains(targetPath)) {
            // If the file exists but is not tracked in the template, we should still add it
            m_currentTemplate.addSequence(targetPath);
        } else {
            QMessageBox::information(this, tr("Add Sequence"), 
                                   tr("Sequence is already in template: %1").arg(targetPath));
        }
    } else {
        // Copy the sequence file to the template directory
        if (!QFile::copy(seqPath, targetPath)) {
            QMessageBox::warning(this, tr("Add Sequence"), 
                               tr("Failed to copy sequence to template directory: %1").arg(targetPath));
            return;
        }
        
        // Add to template
        m_currentTemplate.addSequence(targetPath);
    }

    // Update UI
    updateTemplateSequencesList();
    updateAvailableSequences();
    
    // Refresh the graph to reflect changes
    refreshGraph();
    
    emit templateModified();
}

void TemplateSequenceGraphWidget::onRemoveSequenceFromTemplate()
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
    
    updateAvailableSequences();
    
    // Refresh the graph to reflect changes
    refreshGraph();
    
    emit templateModified();
}

void TemplateSequenceGraphWidget::onSetMainSequence()
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
    
    emit templateModified();
}

void TemplateSequenceGraphWidget::onSequenceInTemplateSelected()
{
    // Update the main sequence button state based on selection
    QListWidgetItem *item = m_templateSequencesList->currentItem();
    m_setMainSequenceBtn->setEnabled(item != nullptr);
}

void TemplateSequenceGraphWidget::onExternalSequenceSelected()
{
    // Update the add sequence button state based on selection
    QListWidgetItem *item = m_availableSequencesList->currentItem();
    m_addSequenceBtn->setEnabled(item != nullptr);
}

void TemplateSequenceGraphWidget::onGraphSequenceSelected(const QString &sequenceName)
{
    emit sequenceSelected(sequenceName);
}

void TemplateSequenceGraphWidget::onSelectImage()
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
        // If we don't have a template name yet, use the current template's name
        QString templateName = m_currentTemplate.name();
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
    m_imageLabel->setText(QFileInfo(targetImagePath).fileName());
    
    // Store relative path from template directory for portability
    QString relativeImagePath = "images/" + QFileInfo(targetImagePath).fileName();
    m_currentTemplate.setImagePath(relativeImagePath);
    m_imagePath = targetImagePath; // Store absolute path for this view
    
    emit templateModified();
}