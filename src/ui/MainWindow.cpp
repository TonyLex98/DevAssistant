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

    // Заменяем обработчик двойного клика
    connect(m_files, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem* item) {

        // Полный путь к файлу
        QString filePath = QDir(m_projectRoot).filePath(item->text());

        // Открываем диалог описания
        DescriptionDialog dlg(filePath, &m_model, &m_dev_manager, this);
        dlg.exec();          // модальный
    });

    auto btns = new QHBoxLayout();

    rescanBtn = new QPushButton("Rescan", this);
    settingsBtn = new QPushButton("Ollama Settings", this);
    promptBtn = new QPushButton("Edit Base Prompt", this);
    applyBtn = new QPushButton("Apply changes", this);
    undoBtn = new QPushButton("Undo", this);

    // ---------- Добавляем кнопку «Generate Descriptions» ----------
    descBtn = new QPushButton("Generate Descriptions", this);

    connect(descBtn, &QPushButton::clicked, this, &MainWindow::generateDescriptions);
    // ----------------------------------------------------------------

    btns->addWidget(rescanBtn);
    btns->addWidget(settingsBtn);
    btns->addWidget(promptBtn);
    btns->addWidget(applyBtn);
    btns->addWidget(undoBtn);
    btns->addWidget(descBtn);
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

void MainWindow::rescan()
{
    /* 0. Подгружаем .dapro перед сканированием */
    m_dev_manager.load();

    /* Путь к каталогу DevAss – будет использоваться для фильтрации */
    const QString devAssPath = m_dev_manager.devAssDir().absolutePath();

    /* Корневой каталог проекта – просто строка, уже абсолютная */
    const QString projectRootPath = m_projectRoot;   // m_projectRoot – QString

    /* Пересканируем индекс проекта */
    m_index.setRoot(m_projectRoot);
    m_files->clear();

    /* Идем по списку файлов проекта */
    for (const QString& rel : m_index.files()) {
        /* Полный абсолютный путь к файлу */
        const QString absPath = QDir(projectRootPath).filePath(rel);

        /* ---  Фильтр: игнорируем файлы из DevAss  --- */
        if (absPath.startsWith(devAssPath))
            continue;            // пропускаем файл, как будто его нет

        /* Создаём элемент списка */
        QListWidgetItem* item = new QListWidgetItem(QDir::fromNativeSeparators(rel), m_files);

        /* Получаем данные из .dapro */
        FileDescription* fd = m_dev_manager.findFileDescription(absPath);
        QDateTime fileMod = QFileInfo(absPath).lastModified();

        /* Отметка цвета: красный – файл не описан / устарел, зелёный – актуален */
        bool needRed = false;
        if (!fd) {
            needRed = true;
        } else if (fileMod > fd->lastModTime) {
            needRed = true;
        }

        if (needRed)
            item->setBackgroundColor(Qt::red);
        else
            item->setBackgroundColor(Qt::green);   // можно оставить default

        /* Храним абсолютный путь в UserRole */
        item->setData(Qt::UserRole, absPath);
    }

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

// MainWindow.cpp
// --------------------------------------------

// MainWindow.cpp
QString MainWindow::buildProjectContext() const
{
    const QString projectRootPath = m_projectRoot;   // QString
    const QString devAssPath = m_dev_manager.devAssDir().absolutePath();

    QString s = "Корневая папка: " + projectRootPath + "\nФайлы:\n";

    int file_counter = 0;
    for (const QString& f : m_index.files()) {
        /* Полный путь */
        const QString abs = QDir(projectRootPath).filePath(f);

        /* Пропускаем файлы из DevAss */
        if (abs.startsWith(devAssPath))
            continue;

        /* Пропускаем файлы, которые лежат в каталоге DevAss
           (это дублирующий фильтр, но обеспечивает надёжность) */
        if (f.startsWith(QStringLiteral("DevAss") + QDir::separator()))
            continue;

        s += " - " + f + '\n';
        file_counter++;
    }

    if(file_counter == 0){
         s += "Файлов в проекте пока нет." + '\n';
    }


    /* Добавляем содержимое файлов, открытых командой OPEN_FILE */
    if (!m_contextContent.isEmpty())
        s += "\nСодержимое файлов, добавленных в контекст:\n" + m_contextContent + "\n";

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

    QString capabilities = "";
    QString full = m_promptMgr.buildPrompt(text, capabilities,buildProjectContext(), m_history);

    m_streamBuf.clear();
    m_model.startStream(full);
    m_history.push_back(qMakePair(QString("user"), text));

    qDebug()<<" ------------- full prompt --------------";
    qDebug()<<full;


}

void MainWindow::onModelChunk(const QString& s){
    m_streamBuf += s;
    if (m_chat) m_chat->appendAssistantChunk(s);
}

QByteArray MainWindow::extractJsonPart(const QByteArray& txt) const
{
    /* Ищем массив … */
    int start = txt.indexOf('[');
    int end   = txt.lastIndexOf(']');

    if (start >= 0 && end >= start) {
        return txt.mid(start, end - start + 1);           // [ … ]
    }

    /* Ищем объект … */
    start = txt.indexOf('{');
    end   = txt.lastIndexOf('}');
    if (start >= 0 && end >= start) {
        QByteArray obj = txt.mid(start, end - start + 1);
        // оборачиваем в массив, чтобы CommandParser работал
        return QByteArray("[") + obj + QByteArray("]");
    }

    // если ничего не найдено – возвращаем пустую строку
    return QByteArray();
}


void MainWindow::handleAssistantFinished()
{
    /* 1. Ответ LLM – полный текст (включая «лишние» строки) */
    QByteArray fullResp = m_streamBuf.toUtf8();

    /* 2. Выбираем только JSON‑часть */
    QByteArray jsonResp = extractJsonPart(fullResp);

    /* 3. Проверяем, что JSON найден */
    if (jsonResp.isEmpty()) {
        log("Assistant ответил, но не содержит JSON‑массив. Команды не применяются.");
        m_history.push_back(qMakePair(QString("assistant"), QString(fullResp)));
        return;
    }

    /* 4. Парсим команды */
    QVector<Command*> cmds = CommandParser::parse(jsonResp);

    /* ... остальная часть функции остаётся без изменений ... */
    //m_history.push_back(qMakePair(QString("assistant"), QString(fullResp)));

    if (cmds.isEmpty()) {
        log("LLM не вернул ни одной команды");
        return;
    }

    /* 5. Подтверждение пользователем */
    CommandConfirmDialog dlg(cmds.toList(), this);
    if (dlg.exec() != QDialog::Accepted) {
        log("Пользователь отклонил команды");
        qDeleteAll(cmds);
        return;
    }
    QList<Command*> toRun = dlg.confirmed();

    /* 6. Применяем команды */
    QString err;
    for (Command* c : toRun) {
        if (!c->apply(&m_dev_manager, &err)) {
            QMessageBox::warning(this, "Ошибка выполнения", err);
            break;
        }

        if (c->name() == "OPEN_FILE") {
            auto* oc = static_cast<OpenFileContextCommand*>(c);
            if (!oc->content().isEmpty()) {
                m_contextContent += "Содержимое файла " + QString("\"") + oc->path() +"\"\n";

                if (!m_contextContent.isEmpty())
                    m_contextContent += "\n---\n";
                m_contextContent += oc->content() + "\n";
            }
        }
        /* … дополнительная логика для OPEN_FILE … */
    }

    /* 7. После применения обновляем список */
    rescan();
    qDeleteAll(toRun);
}

void MainWindow::generateDescriptions()
{
    // 1. Подготовка DevAss
    if (!m_dev_manager.createFolder() || !m_dev_manager.createDaproFile()) {
        QMessageBox::critical(this, "Ошибка",
                              "Не удалось подготовить каталог DevAss.");
        return;
    }

    // 2. Сбор списка файлов проекта (без DevAss)
    QStringList fileList;
    QDirIterator it(m_projectRoot,
                    QDir::Files,
                    QDirIterator::Subdirectories);

    const QString devAssPath = m_dev_manager.devAssDir().absolutePath();

    while (it.hasNext()) {
        const QString fp = it.next();
        if (fp.startsWith(devAssPath))        // игнорируем DevAss
            continue;

        /* --------‑ Фильтр: только если файл ещё не описан ----------
           или описан, но не синхронизирован с изменениями. -------- */
        FileDescription* fd = m_dev_manager.findFileDescription(fp);
        if (fd && fd->isDescriptionUpToDate)
            continue;   // уже актуально

        fileList << fp;
    }

    if (fileList.isEmpty()) {
        QMessageBox::information(this, "Готово",
                                 "В проекте нет файлов, требующих описания.");
        return;
    }

    // 3. Создание прогресс‑диалога
    QProgressDialog prog("Генерация описаний…", "Отмена",
                         0, fileList.size(), this);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(0);
    prog.show();

    int processed = 0, failed = 0;

    for (const QString &srcFilePath : fileList) {
        if (prog.wasCanceled())
            break;

        // 3.1 Чтение содержимого файла
        QFile f(srcFilePath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Не удалось прочитать:" << srcFilePath;
            ++failed; ++processed; prog.setValue(processed); continue;
        }
        const QString content = QString::fromUtf8(f.readAll());
        f.close();

        // 3.2 Формируем русский запрос к модели
        const QString prompt =
            "Пожалуйста, дайте краткое описание следующего содержимого файла.\n"
            "Если содержит код, перечислите основные классы/функции.\n"
            "Если это не код, дайте краткое резюме.\n\n" + content;

        // 3.3 Запрос к Ollama
        const QString description = m_model.generate(prompt);
        if (description.isEmpty()) {
            qWarning() << "Пустое описание для:" << srcFilePath;
            ++failed; ++processed; prog.setValue(processed); continue;
        }

        // 3.4 Путь к файлу‑дескриптору *.dadescr
        const QString rel   = QDir(m_projectRoot).relativeFilePath(srcFilePath);
        const QString descr = m_dev_manager.devAssDir().filePath(rel + ".dadescr");

        // 3.5 Создаём нужные подкаталоги
        QDir().mkpath(QFileInfo(descr).absolutePath());

        QFile df(descr);
        if (!df.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Не удалось записать дескриптор:" << descr;
            ++failed; ++processed; prog.setValue(processed); continue;
        }
        df.write(description.toUtf8());
        df.close();

        // 3.6 Обновляем .dapro: запись времени и флага «актуально»
        QDateTime now = QDateTime::currentDateTime();
        m_dev_manager.updateFileDescription(srcFilePath, now, true);
        m_dev_manager.save();   // запись в .dapro

        ++processed;
        log(QString("✓ Описан %1 → %2")
            .arg(rel, QFileInfo(descr).fileName()));

        prog.setValue(processed);
    }

    prog.setValue(fileList.size());   // закрываем прогресс‑диалог

    log(QString("Генерация завершена: %1 успешно, %2 ошибок.")
        .arg(processed - failed).arg(failed));
}

void MainWindow::log(const QString& m){ m_log->appendPlainText(m); }
