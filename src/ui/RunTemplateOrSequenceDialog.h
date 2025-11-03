#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QToolButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "PromptSequence.h"
#include "SolutionTemplate.h"
#include "customWidgets/MemoryComboBox.h"
#include "SolutionTemplateManager.h"

class RunTemplateOrSequenceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RunTemplateOrSequenceDialog(const QMap<QString, PromptSequence> &seqs,
                                         SolutionTemplateManager *templateManager,
                                         QWidget *parent = nullptr);
    
    enum DialogMode { RunSequence, RunTemplate };
    
    DialogMode selectedMode() const { return m_selectedMode; }
    QString selectedSequencePath() const { return m_selectedSequencePath; }
    QString selectedTemplatePath() const { return m_selectedTemplatePath; }
    QString selectedDefaultSequencePath() const { return m_selectedDefaultSequencePath; }

private slots:
    void onOk();
    void onTemplateSelected(const QString &templateName);
    void onModeChanged(int index);

private:
    void setupUI();
    void populateSequences();
    void populateTemplates();

    QTabWidget *m_tabWidget = nullptr;
    QListWidget *m_sequenceList = nullptr;
    QListWidget *m_templateList = nullptr;
    MemoryComboBox *m_defaultSequenceCombo = nullptr;
    QLabel *m_templateDescriptionLabel = nullptr;
    QLabel *m_templateImageLabel = nullptr;
    
    DialogMode m_selectedMode = RunSequence;
    QString m_selectedSequencePath;
    QString m_selectedTemplatePath;
    QString m_selectedDefaultSequencePath;
    
    const QMap<QString, PromptSequence> &m_seqs;
    SolutionTemplateManager *m_templateManager = nullptr;
};