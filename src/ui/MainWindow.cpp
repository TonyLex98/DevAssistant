#include "MainWindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>

#include <QStatusBar>
#include <QMessageBox>
#include "../ui/ChatDialog.h"
#include "../ui/ModelsDialog.h"
#include "../ui/OllamaSettingsDialog.h"
#include "../ui/PromptEditorDialog.h"
#include "../ui/ChangesDialog.h"
#include "../utils/DslExtractor.h"
#include "../scenarios/ScenarioRegistry.h"
#include <QDir>

MainWindow::MainWindow(QWidget* parent):QMainWindow(parent){
    setWindowTitle("DevAssistantQt");
    QWidget* c = new QWidget(this);
    setCentralWidget(c);
    auto h = new QHBoxLayout(c);

    auto left = new QVBoxLayout();
    h->addLayout(left,1);

    m_rootEdit = new QLineEdit(this);
    browseBtn = new QPushButton("Browse...", this);
    auto top = new QHBoxLayout();
    top->addWidget(m_rootEdit,1);
    top->addWidget(browseBtn);
    left->addLayout(top);

    m_files = new QListWidget(this);
    left->addWidget(m_files,1);

    connect(m_files, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item){

        // Полный путь к файлу (предположим, что item->text() – относительный путь)
        QString filePath = QDir(m_projectRoot).filePath(item->text());

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this,
                                 tr("Не удалось открыть файл"),
                                 tr("Файл %1 не может быть открыт. Ошибка: %2")
                                 .arg(filePath)
                                 .arg(file.errorString()));
            return;
        }

        // Читаем всё содержимое в строку
        QTextStream in(&file);
        // Если вы хотите кодировку, задайте её явно, например:
        // in.setCodec("UTF-8");
        QString fileContent = in.readAll();   // теперь fileContent содержит весь текст

        // Здесь вы можете сделать что‑угодно с содержимым
        // Например, вывести в текстовый виджет:
        // ui->textEdit->setPlainText(fileContent);

        // Или просто вывести в консоль (для отладки)
        qDebug() << "Содержимое файла:" << filePath << "\n" << fileContent;

        QString prompt = "Опиши кратко что находится в фйле, кратко его основные методы функции если это связано с кодом. Если нет то просто краткое описание: \n";

        m_chat->sendMessageToChat(prompt + fileContent);


    });

    auto btns = new QHBoxLayout();

    rescanBtn = new QPushButton("Rescan", this);
    settingsBtn = new QPushButton("Ollama Settings", this);
    promptBtn = new QPushButton("Edit Base Prompt", this);
    applyBtn = new QPushButton("Apply changes", this);
    undoBtn = new QPushButton("Undo", this);

    btns->addWidget(rescanBtn);
    btns->addWidget(settingsBtn);
    btns->addWidget(promptBtn);
    btns->addWidget(applyBtn);
    btns->addWidget(undoBtn);
    left->addLayout(btns);


    m_chat = new ChatDialog(&m_model, this);
    connect(m_chat, &ChatDialog::sendMessage, this, &MainWindow::sendUserMessage);
    connect(&m_model, &OllamaAdapter::streamChunk, this, &MainWindow::onModelChunk);
    connect(&m_model, &OllamaAdapter::streamFinished, this, &MainWindow::handleAssistantFinished);
    connect(&m_model, &OllamaAdapter::streamError, this, [this](const QString& e){ log("Model error: "+e); });

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    left->addWidget(m_log,1);

    connect(browseBtn,&QPushButton::clicked,this,&MainWindow::browse);
    connect(rescanBtn,&QPushButton::clicked,this,&MainWindow::rescan);
    connect(settingsBtn,&QPushButton::clicked,this,&MainWindow::openSettings);
    connect(promptBtn,&QPushButton::clicked,this,&MainWindow::editBasePrompt);
    connect(applyBtn,&QPushButton::clicked,this,&MainWindow::applyChanges);
    connect(undoBtn,&QPushButton::clicked,this,&MainWindow::undoLast);

    statusBar()->showMessage(QString("Model: %1").arg(m_model.selectedModel()));

    m_projectRoot = QDir::homePath();
    m_rootEdit->setText(m_projectRoot);
    //rescan();

    left->addWidget(m_chat);
}

void MainWindow::browse(){
    QString d = QFileDialog::getExistingDirectory(this,"Select project folder", m_projectRoot);
    if (d.isEmpty()) return;
    m_projectRoot = d;
    m_rootEdit->setText(d);
    m_dev_manager.setProjectRoot(m_projectRoot);
    if (!m_dev_manager.createFolder() || !m_dev_manager.createDaproFile())
    {
        qCritical() << "Не удалось подготовить DevAss / .dapro";
        //return -1;
    }

    if (!m_dev_manager.generateDescrFiles())
    {
        qCritical() << "Ошибка при создании .dadescr файлов";
    }

    rescan();
}

void MainWindow::rescan(){
    m_index.setRoot(m_projectRoot);
    m_files->clear();
    for (const QString& f : m_index.files()) m_files->addItem(QDir::fromNativeSeparators(f));
    log("Rescanned project.");
}

void MainWindow::openSettings(){
    OllamaSettingsDialog d(this);
    if (d.exec()==QDialog::Accepted) {
        m_model.setEndpoint(QUrl(d.endpoint()));
        QVariantMap opts = d.params();
        if (!m_model.selectedModel().isEmpty()) m_model.configure(m_model.selectedModel(), opts);
        log("Settings applied.");
    }
}

void MainWindow::editBasePrompt(){
    PromptEditorDialog d(&m_promptMgr, this);
    d.exec();
}

QString MainWindow::buildProjectContext() const {
    QString s = "Root: "+m_projectRoot+"\nFiles:\n";
    for (const QString& f : m_index.files()) s += " - "+f+"\n";
    return s;
}

void MainWindow::applyChanges(){
    if (m_pending.isEmpty()) { QMessageBox::information(this,"Nothing to apply","No pending changes."); return; }
    ChangesDialog dlg(this);
    dlg.setChanges(m_pending);
    if (dlg.exec()==QDialog::Accepted) {
        QString err;
        QList<Change> copy = m_pending;
        if (!m_applier.apply(copy, &err)) { QMessageBox::critical(this,"Apply failed", err); return; }
        log(QString("Applied %1 change(s).").arg(m_pending.size()));
        m_pending.clear();
        rescan();
    } else {
        log("Changes canceled.");
    }
}

void MainWindow::undoLast(){
    QString err; if (!m_applier.undo(&err)) { QMessageBox::warning(this,"Undo", err.isEmpty()?"Nothing to undo":err); }
    else { log("Undo done."); rescan(); }
}

void MainWindow::sendUserMessage(const QString& text){
    QString capabilities = ScenarioRegistry::capabilitiesText();
    QString full = m_promptMgr.buildPrompt(text, buildProjectContext(), capabilities, m_history);

    m_streamBuf.clear();
    m_model.startStream(text);
    m_history.push_back(qMakePair(QString("user"), text));
}

void MainWindow::onModelChunk(const QString& s){
    m_streamBuf += s;
    if (m_chat) m_chat->appendAssistantChunk(s);
}

void MainWindow::handleAssistantFinished(){
    if (m_chat) m_chat->finishAssistantMessage();
    m_history.push_back(qMakePair(QString("assistant"), m_streamBuf));

    QVector<Change> changes;
    QString err = DslExtractor::extractAll(m_streamBuf, m_projectRoot, changes);
    if (!err.isEmpty()) log("Parse: "+err);
    m_pending = changes.toList();
    if (m_pending.isEmpty()) { log("No changes parsed."); return; }
    applyChanges();
}

void MainWindow::log(const QString& m){ m_log->appendPlainText(m); }
