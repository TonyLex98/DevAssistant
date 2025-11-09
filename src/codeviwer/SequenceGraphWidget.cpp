#include "SequenceGraphWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QHBoxLayout>
#include "../core/SolutionTemplateManager.h"
#include "../core/PromptSequenceManager.h"

SequenceGraphWidget::SequenceGraphWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Sequence Relationships Graph");
    setMinimumSize(200, 200);

    // Create the main layout
    m_mainLayout = new QVBoxLayout(this);

    // Create label and combobox for template selection
    m_templateLabel = new QLabel("Select Template:", this);
    m_templateComboBox = new MemoryComboBox("sequenceGraphTemplate", this);
    
    // Create a horizontal layout for the label and combobox
    auto *topLayout = new QHBoxLayout();
    topLayout->addWidget(m_templateLabel);
    topLayout->addWidget(m_templateComboBox);
    topLayout->addStretch(); // Add stretch to push the combo box to the left
    
    // Add the top controls layout to the main layout
    m_mainLayout->addLayout(topLayout);

    // Create the graph view
    m_view = new SequenceRelationView(this);
    m_mainLayout->addWidget(m_view);
    
    // Set the main layout
    setLayout(m_mainLayout);

    // Connect the combobox signal to handle template changes
    connect(m_templateComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &SequenceGraphWidget::onTemplateComboBoxChanged);

    connect(m_view, &SequenceRelationView::sequenceSelected, this, [this](const QString &sequenceName) {
        // This will be connected to the main window to open the sequence editor
        emit sequenceSelected(sequenceName);
    });
    
    qDebug() << "[SequenceGraphWidget] Constructor completed";
}

void SequenceGraphWidget::setSequenceManager(PromptSequenceManager *seqMgr)
{
    qDebug() << "[SequenceGraphWidget] setSequenceManager called";
    m_seqMgr = seqMgr; // Store the sequence manager reference
    m_view->setSequenceManager(seqMgr);
}

void SequenceGraphWidget::setCurrentSequence(const QString &sequenceName)
{
    m_view->setCurrentSequence(sequenceName);
}

void SequenceGraphWidget::setTemplateFilter(const QString &templateDirPath)
{
    qDebug() << "[SequenceGraphWidget] setTemplateFilter called with:" << templateDirPath;
    // In a full implementation, we would use this to filter
    m_view->rebuildGraphForTemplate(templateDirPath);
}

void SequenceGraphWidget::rebuildGraph()
{
    qDebug() << "[SequenceGraphWidget] rebuildGraph called";
    m_view->rebuildGraph();
}

void SequenceGraphWidget::rebuildGraphForTemplate(const QString &templateDirPath)
{
    qDebug() << "[SequenceGraphWidget] rebuildGraphForTemplate called with:" << templateDirPath;
    m_view->rebuildGraphForTemplate(templateDirPath);
}

void SequenceGraphWidget::saveLayout()
{
    qDebug() << "[SequenceGraphWidget] saveLayout called";
    m_view->saveLayout();
}

void SequenceGraphWidget::restoreLayout()
{
    qDebug() << "[SequenceGraphWidget] restoreLayout called";
    m_view->restoreLayout();
}

void SequenceGraphWidget::populateTemplateComboBox()
{
    if (!m_templateComboBox || !m_templateMgr) {
        return;
    }

    // Get list of templates
    const QMap<QString, SolutionTemplate>& templates = m_templateMgr->templates();
    
    // Create a list for the combobox
    QStringList templateNames;
    
    // Add template names to the list
    for (auto it = templates.constBegin(); it != templates.constEnd(); ++it) {
        QString name = it.value().name();
        templateNames << name;
    }
    
    // Add option to show all sequences
    templateNames << "All Sequences";
    
    // Set the items in the combobox
    m_templateComboBox->setItems(templateNames);
    
    // Optionally, set the first item as default if nothing is currently selected
    if (m_templateComboBox->currentIndex() < 0 && m_templateComboBox->count() > 0) {
        m_templateComboBox->setCurrentIndex(0);
    }
    
    qDebug() << "[SequenceGraphWidget] Template combobox populated with:" << templateNames;
}

void SequenceGraphWidget::onTemplateComboBoxChanged()
{
    if (!m_templateComboBox || !m_templateMgr) {
        return;
    }
    
    QString selectedName = m_templateComboBox->currentText();
    qDebug() << "[SequenceGraphWidget] Template selection changed to:" << selectedName;
    
    if (selectedName == "All Sequences") {
        // Rebuild graph without template filter (show all sequences)
        m_view->rebuildGraph();
    } else {
        // Find the template path by name
        const QMap<QString, SolutionTemplate>& templates = m_templateMgr->templates();
        for (auto it = templates.constBegin(); it != templates.constEnd(); ++it) {
            QString path = it.key();  // Get the template directory path (the key is the path)
            QString name = it.value().name();  // Get the template name from the SolutionTemplate object
            
            if (name == selectedName) {
                // Rebuild graph for the selected template
                m_view->rebuildGraphForTemplate(path);
                qDebug() << "[SequenceGraphWidget] Showing sequences for template:" << name << "at path:" << path;
                return;
            }
        }
        
        // If we get here, we couldn't find the template by name, so default to showing all sequences
        qDebug() << "[SequenceGraphWidget] Could not find template for name:" << selectedName << ", showing all sequences";
        m_view->rebuildGraph();
    }
}

void SequenceGraphWidget::setTemplateManager(SolutionTemplateManager *templateMgr)
{
    qDebug() << "[SequenceGraphWidget] setTemplateManager called";
    m_templateMgr = templateMgr; // Store the template manager reference
    if (m_view) {
        m_view->setTemplateManager(templateMgr);
        qDebug() << "[SequenceGraphWidget] Template manager set on view";
    }
    
    // Populate the combobox with available templates
    populateTemplateComboBox();
}
