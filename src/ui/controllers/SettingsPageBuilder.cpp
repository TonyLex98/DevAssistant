#include "SettingsPageBuilder.h"
#include "../customWidgets/StyledButton.h"
#include "../customWidgets/MemoryComboBox.h"
#include "../OllamaSettingsDialog.h"
#include "TtsController.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QSizePolicy>
#include <QObject>
#include <QtGlobal>
#include <QApplication>
#include <qobjectdefs.h>

void SettingsPageBuilder::buildPages(QWidget* parent,
                                     QWidget* projectPathWidget,
                                     TtsController* ttsController,
                                     QWidget* llmSettingsParent)
{
    // ---- Sidebar ----
    m_sidebar = new QListWidget(parent);
    m_sidebar->setMaximumWidth(180);
    m_sidebar->setMinimumWidth(140);
    m_sidebar->setStyleSheet(
        "QListWidget { background: #fff; border: 1px solid #8ed9cf; border-radius: 8px; padding: 6px; }"
        "QListWidget::item { padding: 8px 12px; border-radius: 6px; margin: 2px 0; font: 9pt \"Segoe UI Variable\"; color: #334155; }"
        "QListWidget::item:selected { background: #dff8f3; color: #0f766e; font-weight: 600; }"
        "QListWidget::item:hover { background: #e6f5f1; }"
    );
    m_sidebar->addItem("📁  Проект");
    m_sidebar->addItem("🤖  Модель");
    m_sidebar->addItem("📋  Последовательности");
    m_sidebar->addItem("📊  Графики");
    m_sidebar->addItem("🔊  Аудио");
    m_sidebar->addItem("⚙️  Система");

    // ---- Pages stack ----
    m_pages = new QStackedWidget(parent);
    m_pages->setStyleSheet("QStackedWidget { background: transparent; }");
    QObject::connect(m_sidebar, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);

    // -- Page 0: Project --
    auto* p0 = makePage();
    m_pages->addWidget(p0);
    auto* projLay = pageLay(p0);
    addSection(QObject::tr("Путь к проекту"), projLay);
    projLay->addWidget(projectPathWidget);
    addDiv(projLay);
    addSection(QObject::tr("Действия"), projLay);
    m_rescanBtn = new StyledButton(QObject::tr("Обновить проект"), parent);
    m_descBtn = new StyledButton(QObject::tr("Сгенерировать описания"), parent);
    m_rescanBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_descBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    projLay->addWidget(m_rescanBtn);
    projLay->addWidget(m_descBtn);
    projLay->addStretch();

    // -- Page 1: Model --
    auto* p1 = makePage();
    m_pages->addWidget(p1);
    auto* llmLay = pageLay(p1);
    addSection(QObject::tr("Настройки модели"), llmLay);
    m_settingsBtn = new StyledButton(QObject::tr("Открыть настройки LLM"), parent);
    m_promptBtn = new StyledButton(QObject::tr("Редактировать базовый промпт"), parent);
    llmLay->addWidget(m_settingsBtn);
    llmLay->addWidget(m_promptBtn);
    addDiv(llmLay);
    m_llmSettingsWindow = new OllamaSettingsDialog(llmSettingsParent);
    m_llmSettingsWindow->setVisible(false);
    llmLay->addWidget(m_llmSettingsWindow);
    llmLay->addStretch();

    // -- Page 2: Sequences --
    auto* p2 = makePage();
    m_pages->addWidget(p2);
    auto* seqLay = pageLay(p2);
    addSection(QObject::tr("Последовательности"), seqLay);
    m_editSeqBtn = new StyledButton(QObject::tr("Редактировать"), parent);
    m_newSeqBtn = new StyledButton(QObject::tr("Создать"), parent);
    m_runTemplateBtn = new StyledButton(QObject::tr("Запустить шаблон"), parent);
    m_editTemplatesBtn = new StyledButton(QObject::tr("Редактировать шаблоны"), parent);
    m_exportSequenceAsTemplateBtn = new StyledButton(QObject::tr("Экспорт как шаблон"), parent);
    seqLay->addWidget(m_editSeqBtn);
    seqLay->addWidget(m_newSeqBtn);
    seqLay->addWidget(m_runTemplateBtn);
    seqLay->addWidget(m_editTemplatesBtn);
    seqLay->addWidget(m_exportSequenceAsTemplateBtn);
    seqLay->addStretch();

    // -- Page 3: Graphs --
    auto* p3 = makePage();
    m_pages->addWidget(p3);
    auto* graphLay = pageLay(p3);
    addSection(QObject::tr("Визуализация"), graphLay);
    m_graphBtn = new StyledButton(QObject::tr("Граф контекста"), parent);
    m_seqGraphBtn = new StyledButton(QObject::tr("Граф последовательностей"), parent);
    m_lightRagBtn = new StyledButton(QObject::tr("LightRAG"), parent);
    graphLay->addWidget(m_graphBtn);
    graphLay->addWidget(m_seqGraphBtn);
    graphLay->addWidget(m_lightRagBtn);
    graphLay->addStretch();

    // -- Page 4: Audio --
    auto* p4 = makePage();
    m_pages->addWidget(p4);
    auto* audioLay = pageLay(p4);
    addSection(QObject::tr("Синтез речи (TTS)"), audioLay);
    if (ttsController) {
        audioLay->addWidget(ttsController->toggleButton());
    }
    audioLay->addStretch();

    // -- Page 5: System --
    auto* p5 = makePage();
    m_pages->addWidget(p5);
    auto* sysLay = pageLay(p5);
    addSection(QObject::tr("Безопасность"), sysLay);
    m_confirmModeCombo = new MemoryComboBox("confirmMode", parent);
    m_confirmModeCombo->addItem("Автоподтверждение команд", "auto");
    m_confirmModeCombo->addItem("Спрашивать подтверждение", "ask");
    m_confirmModeCombo->setCurrentIndex(0);
    sysLay->addWidget(m_confirmModeCombo);
    addDiv(sysLay);
    addSection(QObject::tr("Интерфейс"), sysLay);
    m_chatToggleBtn = new StyledButton(QObject::tr("Показать чат"), parent);
    m_logToggleBtn = new StyledButton(QObject::tr("Показать лог"), parent);
    m_chatToggleBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_logToggleBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sysLay->addWidget(m_chatToggleBtn);
    sysLay->addWidget(m_logToggleBtn);
    sysLay->addStretch();

    // Set Expanding policy for all buttons on settings pages
    setAllButtonsExpanding();
}

QWidget* SettingsPageBuilder::makePage()
{
    auto* w = new QWidget();
    w->setStyleSheet("QWidget { background: #f8fffd; }");
    w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(12, 12, 12, 12);
    l->setSpacing(8);
    l->addStretch();
    return w;
}

QVBoxLayout* SettingsPageBuilder::pageLay(QWidget* w)
{
    return static_cast<QVBoxLayout*>(w->layout());
}

void SettingsPageBuilder::addSection(const QString& title, QVBoxLayout* layout)
{
    auto* lb = new QLabel(title);
    lb->setStyleSheet("QLabel { color: #0f766e; font: 600 11pt \"Segoe UI Variable\"; padding: 4px 0; }");
    layout->addWidget(lb);
}

void SettingsPageBuilder::addDiv(QVBoxLayout* layout)
{
    auto* ln = new QFrame();
    ln->setFrameShape(QFrame::HLine);
    ln->setStyleSheet("QFrame { background: #bdebe4; min-height: 1px; max-height: 1px; border: none; }");
    layout->addWidget(ln);
}

void SettingsPageBuilder::setAllButtonsExpanding()
{
    for (int i = 0; i < m_pages->count(); ++i) {
        auto* page = qobject_cast<QWidget*>(m_pages->widget(i));
        if (page) {
            for (auto* btn : page->findChildren<StyledButton*>())
                btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
    }
}
