#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QComboBox>
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
#include <QGroupBox>
#include <QPixmap>
#include <QTextEdit>
#include "PromptSequence.h"
#include "SolutionTemplate.h"
#include "customWidgets/MemoryComboBox.h"
#include "customWidgets/StyledButton.h"
#include "customWidgets/MemoryLineEdit.h"
#include "SolutionTemplateManager.h"
#include "../codeviewer/TemplateSequenceGraphWidget.h"

class SolutionTemplateEditor : public QWidget
{
    Q_OBJECT

public:
    explicit SolutionTemplateEditor(SolutionTemplateManager *templateManager, PromptSequenceManager *sequenceManager, QWidget *parent = nullptr);

    void setTemplate(const SolutionTemplate &templateValue);
    SolutionTemplate templateValue() const;

    // РћР±РЅРѕРІР»РµРЅРёРµ СЃРїРёСЃРєР° РґРѕСЃС‚СѓРїРЅС‹С… РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚РµР№
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
    MemoryLineEdit *m_nameEdit;
    MemoryLineEdit *m_descriptionEdit;
    QLabel *m_imageLabel;
    QLabel *m_imagePathLabel;
    QListWidget *m_templateSequencesList;
    QListWidget *m_availableSequencesList;
    StyledButton *m_addSequenceBtn;
    StyledButton *m_removeSequenceBtn;
    StyledButton *m_setMainSequenceBtn;
    StyledButton *m_selectImageBtn;
    
    // Р”Р»СЏ РѕС‚РѕР±СЂР°Р¶РµРЅРёСЏ РІСЃРµС… РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚РµР№ РёР· РїСЂРѕРµРєС‚Р°
    QMap<QString, QString> m_seqPathByName;  // name -> fullPath РґР»СЏ РґРѕСЃС‚СѓРїРЅС‹С… РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚РµР№
    QString m_imagePath; // РџСѓС‚СЊ Рє РІС‹Р±СЂР°РЅРЅРѕРјСѓ РёР·РѕР±СЂР°Р¶РµРЅРёСЋ
    
    // Graph visualization
    TemplateSequenceGraphWidget *m_graphWidget;
    QTabWidget *m_tabWidget;  // To organize different sections
};
