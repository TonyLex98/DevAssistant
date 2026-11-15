#include "PromptStepDialog.h"
#include "CommandRegistry.h"
#include "PromptResolver.h"   // ← новый заголовок
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QSizePolicy>
#include <QGroupBox>
#include <QTextOption>
#include <QTabWidget>
#include <QTabBar>
#include <QTimer>
#include <QSplitterHandle>
#include <QSignalBlocker>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidgetItem>
#include <StyledButton.h>
#include <QMessageBox>
#include "customWidgets/MemoryLineEdit.h"
#include "customWidgets/StyledListWidget.h"
#include "utils/Logging.h"


PromptStepDialog::PromptStepDialog(QWidget *parent)
    : QWidget(parent)
{
    if (!parent) {
        setWindowTitle(QStringLiteral("Редактирование шага"));
        resize(900, 650);
        setMinimumSize(600, 450);
    }

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->setOpaqueResize(true);
    splitter->setHandleWidth(6);
    splitter->setStyleSheet(
        "QSplitter::handle:horizontal {"
        "  width: 6px;"
        "  background: #c8def8;"
        "  border-left: 1px solid #5A9;"
        "  border-right: 1px solid #5A9;"
        "}"
    );
    root->addWidget(splitter, 1);

    // ---------- Left panel: editor tools ----------
    auto *leftPanel = new QWidget(splitter);
    leftPanel->setMinimumWidth(240);
    auto *leftLay = new QVBoxLayout(leftPanel);
    leftLay->setContentsMargins(4, 4, 4, 4);
    leftLay->setSpacing(6);

    auto *tabs = new QTabWidget(leftPanel);
    tabs->setDocumentMode(true);
    tabs->setUsesScrollButtons(true);
    if (tabs->tabBar())
        tabs->tabBar()->setElideMode(Qt::ElideRight);
    if (tabs->tabBar())
        tabs->tabBar()->setExpanding(false);
    tabs->setStyleSheet(
        "QTabWidget::tab-bar { left: 4px; }"
        "QTabWidget::pane {"
        "  border: 1px solid #8ed9cf;"
        "  border-radius: 4px;"
        "  background-color: #ffffff;"
        "}"
        "QTabBar::tab {"
        "  font: 600 9pt \"Segoe UI Variable\";"
        "  color: #0f172a;"
        "  background-color: #dff8f3;"
        "  border: 1px solid #8ed9cf;"
        "  border-top-left-radius: 4px;"
        "  border-top-right-radius: 4px;"
        "  padding: 4px 8px;"
        "  margin-right: 1px;"
        "  min-width: 60px;"
        "  min-height: 24px;"
        "}"
        "QTabBar::tab:first { margin-left: 1px; }"
        "QTabBar::tab:selected {"
        "  background-color: #ffffff;"
        "  border-bottom-color: #ffffff;"
        "}"
        "QTabBar::tab:hover {"
        "  background-color: #cff3eb;"
        "}"
    );
    leftLay->addWidget(tabs, 1);
    splitter->addWidget(leftPanel);

    // ---------- Right panel: template editor ----------
    auto *rightPanel = new QWidget(splitter);
    rightPanel->setMinimumWidth(280);
    rightPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *rightLay = new QVBoxLayout(rightPanel);
    rightLay->setContentsMargins(6, 6, 6, 6);
    rightLay->setSpacing(6);

    auto *editorTitle = new QLabel(QStringLiteral("Шаблон шага"), rightPanel);
    editorTitle->setStyleSheet("QLabel { font: 13px \"Segoe UI Variable\", sans-serif; font-weight: 600; color: #0f172a; }");
    rightLay->addWidget(editorTitle);

    m_templateEdit = new StyleTextEdit(rightPanel);
    m_templateEdit->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_templateEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    m_templateEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightLay->addWidget(m_templateEdit, 1);
    splitter->addWidget(rightPanel);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    for (int i = 1; i < splitter->count(); ++i) {
        if (auto *h = splitter->handle(i))
            h->setCursor(Qt::SplitHCursor);
    }

    // Set initial sizes proportional to minimums
    QTimer::singleShot(0, this, [splitter]() {
        QList<int> sizes;
        for (int i = 0; i < splitter->count(); ++i) {
            sizes.append(splitter->widget(i)->minimumWidth() + 60);
        }
        splitter->setSizes(sizes);
    });

    // ========== Tab: Inserts ==========
    auto *insertTab = new QWidget(tabs);
    auto *insertLay = new QVBoxLayout(insertTab);
    insertLay->setContentsMargins(6, 6, 6, 6);
    insertLay->setSpacing(6);

    auto *insertHint = new QLabel(QStringLiteral("Вставка переменных в шаблон (формат: {{TOKEN}})."), insertTab);
    insertHint->setWordWrap(true);
    insertLay->addWidget(insertHint);

    m_insertFilter = new MemoryLineEdit("promptStepInsertFilter", insertTab);
    m_insertFilter->setPlaceholderText(QStringLiteral("Фильтр вставок…"));
    insertLay->addWidget(m_insertFilter);

    auto *insertButtons = new QHBoxLayout();
    m_insertBtn = new StyledButton(QStringLiteral("Вставить"), insertTab);
    insertButtons->addWidget(m_insertBtn);
    insertButtons->addStretch(1);
    insertLay->addLayout(insertButtons);

    m_insertList = new StyledListWidget(insertTab);
    m_insertList->setSelectionMode(QAbstractItemView::SingleSelection);
    insertLay->addWidget(m_insertList, 1);

    auto addInsertItem = [this](const QString& title, const QString& token) {
        auto *it = new QListWidgetItem(title, m_insertList);
        it->setData(Qt::UserRole, token);
        it->setToolTip(QString("{{%1}}").arg(token));
    };

    addInsertItem(QStringLiteral("OPEN_SEQUENCE_INFO — переходы/связи"), QStringLiteral("OPEN_SEQUENCE_INFO"));
    for (const auto &info : PromptResolver::availableResolvers())
        addInsertItem(info.description, info.token);
    m_insertList->sortItems(Qt::AscendingOrder);

    auto applyInsertFilter = [this](const QString& text) {
        const QString t = text.trimmed();
        for (int i = 0; i < m_insertList->count(); ++i) {
            QListWidgetItem *it = m_insertList->item(i);
            const QString token = it->data(Qt::UserRole).toString();
            const bool match = t.isEmpty()
                || it->text().contains(t, Qt::CaseInsensitive)
                || token.contains(t, Qt::CaseInsensitive);
            it->setHidden(!match);
        }
    };
    connect(m_insertFilter, &QLineEdit::textChanged, this, applyInsertFilter);
    applyInsertFilter(m_insertFilter->text());

    connect(m_insertBtn, &StyledButton::clicked, this, &PromptStepDialog::onInsertClicked);
    connect(m_insertList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        onInsertClicked();
    });
    connect(m_insertList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *cur, QListWidgetItem*) {
        if (m_insertBtn)
            m_insertBtn->setEnabled(cur != nullptr);
    });
    m_insertBtn->setEnabled(false);
    if (m_insertList->count() > 0) {
        m_insertList->setCurrentRow(0);
        m_insertBtn->setEnabled(true);
    }
    tabs->addTab(insertTab, QStringLiteral("Вставки"));

    // ========== Tab: Commands ==========
    auto *cmdTab = new QWidget(tabs);
    auto *cmdLay = new QVBoxLayout(cmdTab);
    cmdLay->setContentsMargins(6, 6, 6, 6);
    cmdLay->setSpacing(6);

    auto *cmdTitle = new QLabel(QStringLiteral("Разрешённые команды для этого шага (попадают в {{SUPPORTED_COMMANDS}})."), cmdTab);
    cmdTitle->setWordWrap(true);
    cmdLay->addWidget(cmdTitle);

    m_cmdFilter = new MemoryLineEdit("promptStepCommandFilter", cmdTab);
    m_cmdFilter->setPlaceholderText(QStringLiteral("Фильтр команд…"));
    cmdLay->addWidget(m_cmdFilter);

    auto *cmdButtons = new QHBoxLayout();
    auto *cmdSelectAll = new StyledButton(QStringLiteral("Выбрать все"), cmdTab);
    auto *cmdClearAll = new StyledButton(QStringLiteral("Снять все"), cmdTab);
    cmdButtons->addWidget(cmdSelectAll);
    cmdButtons->addWidget(cmdClearAll);
    cmdButtons->addStretch(1);
    cmdLay->addLayout(cmdButtons);

    m_cmdList = new StyledListWidget(cmdTab);
    m_cmdList->setSelectionMode(QAbstractItemView::NoSelection);
    cmdLay->addWidget(m_cmdList, 1);

    for (Command *c : CommandRegistry::all()) {
        QListWidgetItem *it = new QListWidgetItem(c->name(), m_cmdList);
        it->setCheckState(Qt::Unchecked);
        it->setData(Qt::UserRole, c->name());
        delete c;
    }
    m_cmdList->sortItems(Qt::AscendingOrder);

    auto applyCmdFilter = [this](const QString& text) {
        const QString t = text.trimmed();
        for (int i = 0; i < m_cmdList->count(); ++i) {
            QListWidgetItem *it = m_cmdList->item(i);
            const bool match = t.isEmpty() || it->text().contains(t, Qt::CaseInsensitive);
            it->setHidden(!match);
        }
    };
    connect(m_cmdFilter, &QLineEdit::textChanged, this, applyCmdFilter);
    applyCmdFilter(m_cmdFilter->text());

    connect(cmdSelectAll, &QPushButton::clicked, this, [this]() {
        for (int i = 0; i < m_cmdList->count(); ++i)
            m_cmdList->item(i)->setCheckState(Qt::Checked);
    });
    connect(cmdClearAll, &QPushButton::clicked, this, [this]() {
        for (int i = 0; i < m_cmdList->count(); ++i)
            m_cmdList->item(i)->setCheckState(Qt::Unchecked);
    });

    tabs->addTab(cmdTab, QStringLiteral("Команды"));

    // ========== Tab: Sequences ==========
    auto *seqTab = new QWidget(tabs);
    auto *seqLay = new QVBoxLayout(seqTab);
    seqLay->setContentsMargins(6, 6, 6, 6);
    seqLay->setSpacing(6);

    auto *seqTitle = new QLabel(QStringLiteral("Доступные последовательности в шаблоне (связи для {{OPEN_SEQUENCE_INFO}})."), seqTab);
    seqTitle->setWordWrap(true);
    seqLay->addWidget(seqTitle);

    m_seqFilter = new MemoryLineEdit("promptStepSequenceFilter", seqTab);
    m_seqFilter->setPlaceholderText(QStringLiteral("Фильтр последовательностей…"));
    seqLay->addWidget(m_seqFilter);

    m_sequenceListWidget = new StyledListWidget(seqTab);
    m_sequenceListWidget->setSelectionMode(QAbstractItemView::NoSelection);
    seqLay->addWidget(m_sequenceListWidget, 1);

    auto applySeqFilter = [this](const QString& text) {
        if (!m_sequenceListWidget)
            return;
        const QString t = text.trimmed();
        for (int i = 0; i < m_sequenceListWidget->count(); ++i) {
            QListWidgetItem *it = m_sequenceListWidget->item(i);
            const bool match = t.isEmpty() || it->text().contains(t, Qt::CaseInsensitive);
            it->setHidden(!match);
        }
    };
    connect(m_seqFilter, &QLineEdit::textChanged, this, applySeqFilter);

    connect(m_sequenceListWidget, &QListWidget::itemChanged, this, &PromptStepDialog::onSequenceItemChanged);
    loadAvailableSequences();
    applySeqFilter(m_seqFilter->text());

    tabs->addTab(seqTab, QStringLiteral("Переходы"));

    // ---------- Options (bottom) ----------
    auto *options = new QGroupBox(QStringLiteral("Параметры шага"), this);
    auto *optLay = new QVBoxLayout(options);
    optLay->setContentsMargins(8, 8, 8, 8);
    optLay->setSpacing(4);

    m_loopCheck = new QCheckBox(QStringLiteral("Зациклить шаг (повторять до остановки)"), options);
    optLay->addWidget(m_loopCheck);

    m_summarizeCheck = new QCheckBox(QStringLiteral("Сделать саммаризацию перед переходом (резюме выполненной работы)"), options);
    m_summarizeCheck->setChecked(true);
    optLay->addWidget(m_summarizeCheck);

    root->addWidget(options);

    connect(m_templateEdit, &QTextEdit::textChanged, this, &PromptStepDialog::emitEdited);
    connect(m_loopCheck, &QCheckBox::toggled, this, &PromptStepDialog::emitEdited);
    connect(m_cmdList, &QListWidget::itemChanged, this, [this](QListWidgetItem*) { emitEdited(); });

#if 0
    setWindowTitle("Редактирование шага");
    resize(600, 460);

    auto *v = new QVBoxLayout(this);

    /* ---------- Кнопка вставки резолвера ---------- */
    auto *hRes = new QHBoxLayout();
    m_resolverCombo = new MemoryComboBox("promptStepResolver", this);
    m_insertBtn = new StyledButton("Вставить", this);
    hRes->addWidget(m_resolverCombo);
    hRes->addWidget(m_insertBtn);
    hRes->addStretch();
    v->addLayout(hRes);

    /* Заполняем список доступных резолверов (описания) */
    for (const auto &info : PromptResolver::availableResolvers()) {
        m_resolverCombo->addItem(info.description, info.token);
    }
    connect(m_insertBtn, &StyledButton::clicked,
            this, &PromptStepDialog::onInsertClicked);
    /* ------------------------------------------------ */

    m_templateEdit = new StyleTextEdit(this);
    v->addWidget(m_templateEdit, 2);

    /* Список команд с чек‑боксами */
    m_cmdList = new QListWidget(this);
    m_cmdList->setSelectionMode(QAbstractItemView::NoSelection);
    v->addWidget(m_cmdList, 1);

    /* Заполняем списком всех известных команд */
    for (Command *c : CommandRegistry::all()) {
        QListWidgetItem *it = new QListWidgetItem(c->name(), m_cmdList);
        it->setCheckState(Qt::Unchecked);
        it->setData(Qt::UserRole, c->name());
        delete c; // шаблон нам больше не нужен
    }

    /* ---------- чек‑бокс «Зациклить шаг» ---------- */
    m_loopCheck = new QCheckBox("Зациклить шаг (повторять до остановки)", this);
    v->addWidget(m_loopCheck);

    /* ---------- чек‑бокс «Сделать саммаризацию» ---------- */
    m_summarizeCheck = new QCheckBox("Сделать саммаризацию перед переходом (резюме выполненной работы)", this);
    m_summarizeCheck->setChecked(true); // Default to checked
    v->addWidget(m_summarizeCheck);

    /* ---------- Кнопка добавления резолвера OPEN_SEQUENCE_INFO ---------- */
    m_addSequenceInfoBtn = new StyledButton("Вставить {{OPEN_SEQUENCE_INFO}}", this);
    v->addWidget(m_addSequenceInfoBtn);
    connect(m_addSequenceInfoBtn, &StyledButton::clicked, this, &PromptStepDialog::onAddSequenceInfo);
    
    /* ---------- Добавляем список доступных последовательностей с чекбоксами ---------- */
    QLabel *seqLabel = new QLabel("Доступные последовательности в шаблоне:", this);
    v->addWidget(seqLabel);
    
    m_sequenceListWidget = new QListWidget(this);
    m_sequenceListWidget->setSelectionMode(QAbstractItemView::NoSelection);
    v->addWidget(m_sequenceListWidget, 0); // Small stretch factor so it doesn't take too much space
    
    /* ---------- Загружаем доступные последовательности ---------- */
    loadAvailableSequences();
    
    /* ---------- Подключаем сигнал для отслеживания изменений в списке ---------- */
    connect(m_sequenceListWidget, &QListWidget::itemChanged, this, &PromptStepDialog::onSequenceItemChanged);

    /* Кнопки ОК/Отмена */
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    v->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &PromptStepDialog::onOk);
    connect(bb, &QDialogButtonBox::rejected, this, &PromptStepDialog::reject);
#endif
}

PromptStep PromptStepDialog::step() const
{
    return buildStepFromUi();
}

void PromptStepDialog::focusTemplate()
{
    if (m_templateEdit)
        m_templateEdit->setFocus();
}

void PromptStepDialog::setStep(const PromptStep &step)
{
    const QSignalBlocker b1(m_templateEdit);
    const QSignalBlocker b2(m_cmdList);
    const QSignalBlocker b3(m_loopCheck);
    const QSignalBlocker b4(m_summarizeCheck);

    m_step = step;
    m_templateEdit->setPlainText(step.tmpl());

    /* Ставим чек‑боксы согласно списку разрешённых команд */
    for (int i = 0; i < m_cmdList->count(); ++i) {
        QListWidgetItem *it = m_cmdList->item(i);
        QString name = it->data(Qt::UserRole).toString();
        it->setCheckState(step.allowedCommands().contains(name) ?
                          Qt::Checked : Qt::Unchecked);
    }

    /* флаг зацикливания */
    m_loopCheck->setChecked(step.loop());
    
    /* По умолчанию включаем саммаризацию (если не указано иное в шаблоне) */
    m_summarizeCheck->setChecked(true);
    // TODO: Parse template to check if summarize flag is set in CALL_SEQUENCE command

    // Refresh sequences list since it depends on template contents.
    loadAvailableSequences();
}

void PromptStepDialog::onInsertClicked()
{
    if (!m_templateEdit) return;
    QString token;
    if (m_insertList && m_insertList->currentItem())
        token = m_insertList->currentItem()->data(Qt::UserRole).toString();
    token = token.trimmed();
    if (token.isEmpty())
        return;
    QTextCursor cur = m_templateEdit->textCursor();
    cur.insertText("{{" + token + "}}");
    m_templateEdit->setTextCursor(cur);
}

void PromptStepDialog::setSequenceManager(PromptSequenceManager* seqMgr)
{
    m_seqMgr = seqMgr;
    // Reload available sequences after sequence manager is set
    loadAvailableSequences();
}

void PromptStepDialog::setCurrentSequencePath(const QString& sequencePath)
{
    if (!sequencePath.isEmpty()) {
        QFileInfo fileInfo(sequencePath);
        m_templateDir = fileInfo.absolutePath(); // Use the directory of the current sequence
        
        // Use the base filename (without extension) as the sequence name for targeting purposes
        // This ensures consistency with the sequenceTargets structure which uses file base names
        m_currentSequenceName = fileInfo.baseName();
    } else {
        m_templateDir.clear();
        m_currentSequenceName.clear();
    }
    // Reload available sequences after template directory and sequence name are set/changed
    loadAvailableSequences();
}

void PromptStepDialog::setCurrentSequenceName(const QString& sequenceName)
{
    m_currentSequenceName = sequenceName;
    // Reload available sequences after sequence name is set/changed
    loadAvailableSequences();
}

PromptStep PromptStepDialog::buildStepFromUi() const
{
    PromptStep out = m_step;
    if (m_templateEdit)
        out.setTemplate(m_templateEdit->toPlainText());

    QStringList allowed;
    if (m_cmdList) {
        for (int i = 0; i < m_cmdList->count(); ++i) {
            QListWidgetItem *it = m_cmdList->item(i);
            if (it->checkState() == Qt::Checked)
                allowed << it->data(Qt::UserRole).toString();
        }
    }
    out.setAllowedCommands(allowed);

    if (m_loopCheck)
        out.setLoop(m_loopCheck->isChecked());

    return out;
}

void PromptStepDialog::emitEdited()
{
    m_step = buildStepFromUi();
    emit stepEdited(m_step);
}



void PromptStepDialog::onAddSequenceInfo()
{
    if (!m_templateEdit) return;
    
    QTextCursor cur = m_templateEdit->textCursor();
    cur.insertText("{{OPEN_SEQUENCE_INFO}}");
    m_templateEdit->setTextCursor(cur);
}

void PromptStepDialog::onSequenceItemChanged(QListWidgetItem *item)
{
    Q_UNUSED(item);
    
    // Update the main template file based on the checked items
    if (!m_templateDir.isEmpty() && QDir(m_templateDir).exists()) {
        QString mainTemplatePath = QDir(m_templateDir).filePath("template.templ.json");
        
        if (QFile::exists(mainTemplatePath)) {
            // Load the existing template
            QFile templateFile(mainTemplatePath);
            if (!templateFile.open(QIODevice::ReadOnly)) {
                return;
            }
            
            QByteArray templateData = templateFile.readAll();
            QJsonDocument templateDoc = QJsonDocument::fromJson(templateData);
            templateFile.close();
            
            if (!templateDoc.isObject()) {
                return;
            }
            
            QJsonObject templateObj = templateDoc.object();
            
            // Create a map of sequence targets from the UI
            QJsonObject sequenceTargets = templateObj.value("sequenceTargets").toObject();
            
            // For the current sequence, update its targets
            QJsonArray currentTargets;
            
            for (int i = 0; i < m_sequenceListWidget->count(); ++i) {
                QListWidgetItem *listItem = m_sequenceListWidget->item(i);
                if (listItem->checkState() == Qt::Checked) {
                    QString seqFileName = listItem->data(Qt::UserRole).toString();
                    
                    // Add to current sequence's targets
                    currentTargets.append(seqFileName);
                }
            }
            
            // Store the targets for the current sequence
            if (!m_currentSequenceName.isEmpty()) {
                if (!currentTargets.isEmpty()) {
                    // Save targets for this sequence
                    sequenceTargets[m_currentSequenceName] = currentTargets;
                } else {
                    // If no targets, remove the entry if it exists
                    if (sequenceTargets.contains(m_currentSequenceName)) {
                        sequenceTargets.remove(m_currentSequenceName);
                    }
                }
            }
            
            templateObj["sequenceTargets"] = sequenceTargets;
            
            // Write the updated template back to the file
            QJsonDocument updatedDoc(templateObj);
            QFile outFile(mainTemplatePath);
            if (outFile.open(QIODevice::WriteOnly)) {
                outFile.write(updatedDoc.toJson(QJsonDocument::Indented));
                outFile.close();
            } else {
            }
        } else {
            
            // Create a basic template file structure if it doesn't exist
            QJsonObject templateObj;
            templateObj["name"] = QFileInfo(m_templateDir).fileName();  // Use directory name as template name
            templateObj["description"] = "Template for sequences in this directory";
            
            // Create sequence targets if we have a current sequence and checked items
            QJsonObject sequenceTargets;
            
            if (!m_currentSequenceName.isEmpty()) {
                QJsonArray currentTargets;
                
                for (int i = 0; i < m_sequenceListWidget->count(); ++i) {
                    QListWidgetItem *listItem = m_sequenceListWidget->item(i);
                    if (listItem->checkState() == Qt::Checked) {
                        QString seqFileName = listItem->data(Qt::UserRole).toString();
                        currentTargets.append(seqFileName);
                    }
                }
                
                if (!currentTargets.isEmpty()) {
                    sequenceTargets[m_currentSequenceName] = currentTargets;
                }
            }
            
            if (!sequenceTargets.isEmpty()) {
                templateObj["sequenceTargets"] = sequenceTargets;
            }
            
            // Write the new template structure
            QJsonDocument newDoc(templateObj);
            QFile outFile(mainTemplatePath);
            if (outFile.open(QIODevice::WriteOnly)) {
                outFile.write(newDoc.toJson(QJsonDocument::Indented));
                outFile.close();
            } else {
            }
        }
    } else {
    }
}

void PromptStepDialog::loadAvailableSequences()
{
    
    if (!m_sequenceListWidget) {
        return;
    }

    const QSignalBlocker blockList(m_sequenceListWidget);
    m_sequenceListWidget->clear();
    
    // Get the current template content to check for existing references
    QString currentTemplate = m_templateEdit ? m_templateEdit->toPlainText() : QString();
    
    // Look for the main template file to read sequence links
    bool hasMainTemplate = false;
    if (!m_templateDir.isEmpty() && QDir(m_templateDir).exists()) {
        QString mainTemplatePath = QDir(m_templateDir).filePath("template.templ.json");
        
        if (QFile::exists(mainTemplatePath)) {
            // Load the main template to get sequence links
            QFile templateFile(mainTemplatePath);
            if (templateFile.open(QIODevice::ReadOnly)) {
                QByteArray templateData = templateFile.readAll();
                QJsonDocument templateDoc = QJsonDocument::fromJson(templateData);
                
                if (templateDoc.isObject()) {
                    QJsonObject templateObj = templateDoc.object();
                    
                    // Look for sequence targets (directed links) in the template
                    if (templateObj.contains("sequenceTargets")) {
                        hasMainTemplate = true;
                        
                        
                        // Get targets for the current sequence (directed links)
                        QJsonArray currentSequenceTargets;
                        if (!m_currentSequenceName.isEmpty() && templateObj.contains("sequenceTargets")) {
                            QJsonObject sequenceTargets = templateObj.value("sequenceTargets").toObject();
                            if (sequenceTargets.contains(m_currentSequenceName)) {
                                currentSequenceTargets = sequenceTargets.value(m_currentSequenceName).toArray();
                                
                                for (int i = 0; i < currentSequenceTargets.size(); ++i) {
                                }
                            } else {
                            }
                        } else {
                        }
                        
                        // Get all sequence files in the directory
                        QDir dir(m_templateDir);
                        QStringList filters = {"*.seq.json"};
                        QFileInfoList fileInfoList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);
                        
                        // Process each sequence file and check if it should be checked based on targets
                        QStringList targetSequenceFiles;  // List of sequence files that are targets of current sequence
                        for (const QJsonValue& targetValue : currentSequenceTargets) {
                            targetSequenceFiles.append(targetValue.toString());
                        }
                        
                        // Create a map of file info for faster lookup
                        QMap<QString, QFileInfo> fileInfoMap;
                        for (const QFileInfo& fileInfo : fileInfoList) {
                            QString baseName = fileInfo.baseName();
                            fileInfoMap[baseName] = fileInfo;
                        }
                        
                        // First add the target sequences (checked)
                        for (const QString& targetFile : targetSequenceFiles) {
                            if (fileInfoMap.contains(targetFile)) {
                                QFileInfo fileInfo = fileInfoMap[targetFile];
                                
                                // Load the sequence from file to get its name and description
                                QFile seqFile(fileInfo.absoluteFilePath());
                                if (!seqFile.open(QIODevice::ReadOnly)) {
                                    continue;
                                }
                                
                                QByteArray data = seqFile.readAll();
                                QJsonDocument doc = QJsonDocument::fromJson(data);
                                if (!doc.isObject()) {
                                    continue;  // Skip invalid JSON files
                                }
                                
                                PromptSequence seq = PromptSequence::fromJson(doc.object());
                                
                                QString display = seq.name();
                                if (!seq.description().isEmpty())
                                    display += " – " + seq.description();
                                    
                                QListWidgetItem *item = new QListWidgetItem(display, m_sequenceListWidget);
                                item->setData(Qt::UserRole, targetFile);  // Store the filename without extension
                                item->setCheckState(Qt::Checked);  // Target sequences are checked
                                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                                
                            }
                        }
                        
                        // Then add the non-target sequences (unchecked)
                        for (const QFileInfo& fileInfo : fileInfoList) {
                            QString seqBaseName = fileInfo.baseName();  // Get filename without extension
                            
                            // Skip if this sequence file is already added as a target
                            if (targetSequenceFiles.contains(seqBaseName)) {
                                continue;  // Already added above as checked
                            }
                            
                            // Load the sequence from file to get its name and description
                            QFile seqFile(fileInfo.absoluteFilePath());
                            if (!seqFile.open(QIODevice::ReadOnly)) {
                                continue;
                            }
                            
                            QByteArray data = seqFile.readAll();
                            QJsonDocument doc = QJsonDocument::fromJson(data);
                            if (!doc.isObject()) {
                                continue;  // Skip invalid JSON files
                            }
                            
                            PromptSequence seq = PromptSequence::fromJson(doc.object());
                            
                            QString display = seq.name();
                            if (!seq.description().isEmpty())
                                display += " – " + seq.description();
                                
                            QListWidgetItem *item = new QListWidgetItem(display, m_sequenceListWidget);
                            item->setData(Qt::UserRole, seqBaseName);  // Store the filename without extension
                            item->setCheckState(Qt::Unchecked);        // Non-target sequences are unchecked
                            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                            
                        }
                    }
                }
            }
        }
    }
    
    // If no main template with links was found, fallback to all sequences in the directory
    if (!hasMainTemplate) {
        // Load sequences directly from the template directory only
        if (!m_templateDir.isEmpty() && QDir(m_templateDir).exists()) {
            QDir dir(m_templateDir);
            QStringList filters = {"*.seq.json"};
            QFileInfoList fileInfoList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);
            
            
            for (const QFileInfo& fileInfo : fileInfoList) {
                QString seqPath = fileInfo.absoluteFilePath();
                
                // Load the sequence from file to get its name and description
                QFile seqFile(seqPath);
                if (!seqFile.open(QIODevice::ReadOnly)) {
                    continue;
                }
                
                QByteArray data = seqFile.readAll();
                QJsonDocument doc = QJsonDocument::fromJson(data);
                if (!doc.isObject()) {
                    continue;  // Skip invalid JSON files
                }
                
                PromptSequence seq = PromptSequence::fromJson(doc.object());
                
                QString display = seq.name();
                if (!seq.description().isEmpty())
                    display += " – " + seq.description();
                    
                QListWidgetItem *item = new QListWidgetItem(display, m_sequenceListWidget);
                item->setData(Qt::UserRole, fileInfo.baseName());  // Store the filename without extension
                
                // For the new OPEN_SEQUENCE_INFO system, we could consider all sequences 
                // as "available" in the template, but let's check if the template contains the resolver
                if (currentTemplate.contains("{{OPEN_SEQUENCE_INFO}}")) {
                    // If the template already has OPEN_SEQUENCE_INFO, consider all sequences as "included"
                    item->setCheckState(Qt::Checked);
                } else {
                    // Otherwise start with unchecked
                    item->setCheckState(Qt::Unchecked);
                }
                
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                
            }
        } else {
            
            // Try to determine template directory from the sequence manager's root or common template locations
            if (m_seqMgr) {
                QString seqMgrRoot = m_seqMgr->root();
                
                // Check if this is in a template directory structure
                QDir checkDir(seqMgrRoot);
                if (checkDir.exists()) {
                    // Look for template directories within the sequence manager root
                    QFileInfoList dirList = checkDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
                    
                    for (const QFileInfo& dirInfo : dirList) {
                        if (dirInfo.fileName() != "PromptSequences") {  // Skip the main sequences folder
                            QDir templateDir(dirInfo.absoluteFilePath());
                            QStringList filters = {"*.seq.json"};
                            QFileInfoList fileInfoList = templateDir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);
                            
                            if (!fileInfoList.isEmpty()) {
                                
                                for (const QFileInfo& fileInfo : fileInfoList) {
                                    QString seqPath = fileInfo.absoluteFilePath();
                                    
                                    QFile seqFile(seqPath);
                                    if (!seqFile.open(QIODevice::ReadOnly)) {
                                        continue;
                                    }
                                    
                                    QByteArray data = seqFile.readAll();
                                    QJsonDocument doc = QJsonDocument::fromJson(data);
                                    if (!doc.isObject()) {
                                        continue;  // Skip invalid JSON files
                                    }
                                    
                                    PromptSequence seq = PromptSequence::fromJson(doc.object());
                                    QString display = seq.name();
                                    if (!seq.description().isEmpty())
                                        display += " – " + seq.description();
                                        
                                    QListWidgetItem *item = new QListWidgetItem(display, m_sequenceListWidget);
                                    item->setData(Qt::UserRole, fileInfo.baseName());  // Store the filename without extension
                                    
                                    // Check if the template already has OPEN_SEQUENCE_INFO
                                    if (currentTemplate.contains("{{OPEN_SEQUENCE_INFO}}")) {
                                        item->setCheckState(Qt::Checked);
                                    } else {
                                        item->setCheckState(Qt::Unchecked);
                                    }
                                    
                                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Re-apply filter after repopulating the list.
    if (m_seqFilter) {
        const QString t = m_seqFilter->text().trimmed();
        for (int i = 0; i < m_sequenceListWidget->count(); ++i) {
            QListWidgetItem *it = m_sequenceListWidget->item(i);
            const bool match = t.isEmpty() || it->text().contains(t, Qt::CaseInsensitive);
            it->setHidden(!match);
        }
    }

}
