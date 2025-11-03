#pragma once

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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
    
    SolutionTemplateManager* m_templateManager;
    QListWidget* m_templateList;
    QLabel* m_templateNameLabel;
    QLabel* m_templateDescriptionLabel;
    QLabel* m_templateImageLabel;
    QPushButton* m_editTemplateBtn;
    QPushButton* m_deleteTemplateBtn;
    QPushButton* m_addTemplateBtn;
    QPushButton* m_exportTemplateBtn;
    QPushButton* m_importSequenceBtn;
    MemoryComboBox* m_defaultSequenceCombo; // Для выбора последовательности по умолчанию
    QPushButton* m_runTemplateBtn;          // Для запуска шаблона с выбранной последовательностью
    QString m_currentTemplateDir;
};