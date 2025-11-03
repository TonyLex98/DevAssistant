#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
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
#include <QList>
#include <QMap>
#include <QVector>
#include <QDirIterator>
#include <QGridLayout>
#include <QPushButton>
#include <QGroupBox>
#include <QPixmap>
#include <QTextEdit>
#include "PromptSequence.h"
#include "SolutionTemplate.h"
#include "customWidgets/MemoryComboBox.h"
#include "SolutionTemplateManager.h"
#include "../codeviwer/TemplateSequenceGraphWidget.h"

class SolutionTemplateEditor : public QWidget
{
    Q_OBJECT

public:
    explicit SolutionTemplateEditor(SolutionTemplateManager *templateManager, PromptSequenceManager *sequenceManager, QWidget *parent = nullptr);

    void setTemplate(const SolutionTemplate &templateValue);
    SolutionTemplate templateValue() const;

    // Обновление списка доступных последовательностей
    void updateAvailableSequences();

signals:
    void templateModified();

private slots:
    void onAddSequence();
    void onRemoveSequence();
    void onSetMainSequence();
    void onSelectImage();
    void onSequenceSelectionChanged();
    void onAvailableSequenceSelectionChanged();
    void onTemplateModified();

private:
    void setupUI();

    SolutionTemplateManager *m_templateManager;
    PromptSequenceManager *m_sequenceManager;
    SolutionTemplate m_currentTemplate;
    MemoryComboBox *m_nameEdit;
    QLineEdit *m_descriptionEdit;
    QLabel *m_imageLabel;
    QLabel *m_imagePathLabel;
    QListWidget *m_templateSequencesList;
    QListWidget *m_availableSequencesList;
    QPushButton *m_addSequenceBtn;
    QPushButton *m_removeSequenceBtn;
    QPushButton *m_setMainSequenceBtn;
    QPushButton *m_selectImageBtn;
    
    // Для отображения всех последовательностей из проекта
    QMap<QString, QString> m_seqPathByName;  // name -> fullPath для доступных последовательностей
    QString m_imagePath; // Путь к выбранному изображению
    
    // Graph visualization
    TemplateSequenceGraphWidget *m_graphWidget;
    QTabWidget *m_tabWidget;  // To organize different sections
};