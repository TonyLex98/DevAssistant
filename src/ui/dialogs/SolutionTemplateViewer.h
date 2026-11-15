#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialog>
#include <QDir>
#include <QStandardPaths>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QPixmap>
#include <QScrollArea>
#include "core/workflow/SolutionTemplateManager.h"
#include "ui/widgets/MemoryComboBox.h"
#include "ui/widgets/StyledButton.h"
#include "ui/widgets/StyledListWidget.h"
#include "ui/widgets/StyledLabel.h"

class SolutionTemplateViewer : public QWidget
{
    Q_OBJECT

public:
    explicit SolutionTemplateViewer(SolutionTemplateManager* templateManager, QWidget *parent = nullptr);

public slots:
    void refreshTemplates();
    void onTemplateSelected(int index);
    void onEditTemplate();
    void onDeleteTemplate();
    void onAddTemplate();
    void onExportTemplate();
    void onImportSequenceAsTemplate();

private slots:
    void updateTemplateList();

private:
    void setupUI();
    void loadTemplateInfo(const QString& templateDir);
    QString wrapTextByCharacterCount(const QString& text, int maxCharsPerLine);

    SolutionTemplateManager* m_templateManager;
    StyledListWidget* m_templateList;
    StyledLabel* m_templateNameLabel;
    StyledLabel* m_templateDescriptionLabel;
    StyledLabel* m_templateImageLabel;
    StyledButton* m_editTemplateBtn;
    StyledButton* m_deleteTemplateBtn;
    StyledButton* m_addTemplateBtn;
    StyledButton* m_exportTemplateBtn;
    StyledButton* m_importSequenceBtn;
    MemoryComboBox* m_defaultSequenceCombo; // default sequence selector
    StyledButton* m_runTemplateBtn;          // runs the template with the selected sequence
    QString m_currentTemplateDir;
    static const int MAX_CHARS_PER_LINE = 50; // max characters per description line
};