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
#include "SolutionTemplateManager.h"
#include "customWidgets/MemoryComboBox.h"
#include "customWidgets/StyledButton.h"
#include "customWidgets/StyledListWidget.h"
#include "customWidgets/StyledLabel.h"

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
    MemoryComboBox* m_defaultSequenceCombo; // Для выбора последовательности по умолчанию
    StyledButton* m_runTemplateBtn;          // Для запуска шаблона с выбранной последовательностью
    QString m_currentTemplateDir;
    static const int MAX_CHARS_PER_LINE = 50; // Максимальное количество символов в строке описания
};