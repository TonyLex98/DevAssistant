#include "SequenceGraphWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QComboBox>
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
    StyledButton *fitAllBtn = new StyledButton(tr("Поместить всё"), this);
    fitAllBtn->setFixedHeight(26);
    fitAllBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    topLayout->addWidget(fitAllBtn);
    topLayout->addStretch(); // Add stretch to push the combo box to the left
    connect(fitAllBtn, &StyledButton::clicked, this, [this]() {
        if (m_view)
            m_view->fitToContents();
    });
    
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

    QString previousSelection = m_currentTemplatePath;
    m_templateComboBox->blockSignals(true);
    m_templateComboBox->clear();

    m_templateComboBox->addItem(tr("All Sequences"), QString());

    const QMap<QString, SolutionTemplate>& templates = m_templateMgr->templates();
    for (auto it = templates.constBegin(); it != templates.constEnd(); ++it) {
        QString name = it.value().name();
        QString path = it.key();
        m_templateComboBox->addItem(name, path);
    }

    int index = 0;
    if (!previousSelection.isEmpty()) {
        index = m_templateComboBox->findData(previousSelection);
        if (index < 0)
            index = 0;
    }
    m_templateComboBox->setCurrentIndex(index);
    m_templateComboBox->blockSignals(false);
    onTemplateComboBoxChanged();
    
    qDebug() << "[SequenceGraphWidget] Template combobox populated, items count:" << m_templateComboBox->count();
}

void SequenceGraphWidget::onTemplateComboBoxChanged()
{
    if (!m_templateComboBox || !m_templateMgr) {
        return;
    }
    
    QString selectedPath = m_templateComboBox->currentData().toString();
    m_currentTemplatePath = selectedPath;
    qDebug() << "[SequenceGraphWidget] Template selection changed to data path:" << selectedPath;
    
    if (selectedPath.isEmpty()) {
        m_view->rebuildGraph();
    } else {
        m_view->rebuildGraphForTemplate(selectedPath);
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

void SequenceGraphWidget::setTemplateSelectionByPath(const QString &templatePath)
{
    if (!m_templateComboBox || m_templateComboBox->count() == 0)
        return;

    QString target = templatePath;
    if (target.isEmpty())
        target.clear();

    int index = target.isEmpty() ? 0 : m_templateComboBox->findData(target);
    if (index < 0)
        index = 0;

    if (index != m_templateComboBox->currentIndex()) {
        m_templateComboBox->setCurrentIndex(index);
    } else {
        onTemplateComboBoxChanged();
    }
}

QString SequenceGraphWidget::currentTemplatePath() const
{
    return m_currentTemplatePath;
}
