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
    // -----------------------------------------------------------------

    // в MainWindow.cpp  (конструктор)
    runSeqBtn = new QPushButton("Run sequence…", this);
    btns->addWidget(runSeqBtn);
    connect(runSeqBtn, &QPushButton::clicked, this, &MainWindow::runSequenceDialog);

    // В конструкторе MainWindow добавить кнопку
    editSeqBtn = new QPushButton("Edit sequences…", this);
    btns->addWidget(editSeqBtn);
    connect(editSeqBtn, &QPushButton::clicked, this, &MainWindow::editSequences);



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

    // *** Добавляем VoskWidget на левую панель ***
    m_voskWidget = new VoskWidget();
    connect(m_voskWidget, &VoskWidget::voiceRecognized, this, &MainWindow::onVoskText);
    //left->addWidget(m_voskWidget,1);

    left->addWidget(m_chat,1);

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

    // <‑‑ Инициализируем VoskWidget и показываем его
    // m_voskWidget = new VoskWidget();
    // connect(m_voskWidget, &VoskWidget::voiceRecognized, this, &MainWindow::onVoskText);
    // m_voskWidget->show();

    // Папка, где будем хранить наборы
    QString seqRoot = QDir(m_dev_manager.devAssDir())
                        .filePath("PromptSequences");
    m_seqMgr.setRoot(seqRoot);
    m_seqMgr.refresh();
    ensureDefaultSequence();

}

void MainWindow::browse(){
    QString d = QFileDialog::getExistingDirectory(this,"Select project folder", m_projectRoot);
    if (d.isEmpty()) return;
    m_projectRoot = d;
    m_rootEdit->setText(d);
    m_dev_manager.setProjectRoot(m_projectRoot);
    if (!m_dev_manager.createFolder() || !m_dev_manager.createDaproFile())
    {
        qCritical() << "Не удалось подготовить DevelopersAssistant / .dapro";
        //return -1;
    }

    if (!m_dev_manager.generateDescrFiles())
    {
        qCritical() << "Ошибка при создании .dadescr файлов";
    }

    rescan();
}

void MainWindow::rescan(){
    /* 0. Подгружаем .dapro перед сканированием */
    m_dev_manager.load();

    /* Путь к каталогу DevelopersAssistant – будет использоваться для фильтрации */
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

        /* ---  Фильтр: игнорируем файлы из DevelopersAssistant  --- */
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
// ------------------------------------------

QString MainWindow::buildProjectContext() const
{
    const QString projectRootPath = m_projectRoot;   // QString
    const QString devAssPath = m_dev_manager.devAssDir().absolutePath();

    QString s = "Корневая папка: " + projectRootPath + "\nФайлы:\n";

    int file_counter = 0;
    for (const QString& f : m_index.files()) {
        const QString abs = QDir(projectRootPath).filePath(f);

        // --- пропускаем файлы из DevelopersAssistant и .dadescr
        if (abs.startsWith(devAssPath) ||
            f.startsWith(QStringLiteral("DevelopersAssistant") + QDir::separator()) ||
            f.endsWith(QStringLiteral(".dadescr")))
            continue;

        s += " - " + f + '\n';
        ++file_counter;

        // ---------- 1. Считаем описание (если надо) ----------
        // В этом примере описания оставляем как есть – но можно удалить
        // или закомментировать блок ниже, если они вам не нужны.

        const QString rel = QDir(projectRootPath).relativeFilePath(abs);
        const QString descrPath =
            m_dev_manager.devAssDir().filePath(rel + ".dadescr");
        QFile descrFile(descrPath);
        if (descrFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString descr = QString::fromUtf8(descrFile.readAll());
            descrFile.close();

            if (!descr.isEmpty()) {
                s += "   Описание:\n";
                s += "   " + descr.replace("\n", "\n   ") + "\n";
            }
        }

        // ---------- 2. ЗАГРУЖАЕМ СОДЕРЖИМОЕ файла ----------
        /*QFile fContent(abs);
        if (fContent.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(fContent.readAll());
            fContent.close();

            if (!content.isEmpty()) {
                s += "   Содержимое:\n";
                s += "   " + content.replace("\n", "\n   ") + "\n";
            }
        }*/
    }

    if(file_counter == 0){
        s += "Файлов в проекте пока нет." + '\n';
    }

    // ---------- Контент файлов, открытых командой OPEN_FILE ----------
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
        log(QString("Applied %1 change(s). ").arg(m_pending.size()));
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

    qDebug()<<" ------------- full prompt --------------";
    qDebug()<<full;

    m_streamBuf.clear();
    m_model.startStream(full);
    m_history.push_back(qMakePair(QString("user"), text));

    // ***  Оценка количества токенов ***
    int tokens = m_promptMgr.tokenCount(full);
    log(QString("Отправляем запрос (%1 токенов) → %2").arg(tokens).arg(text));


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

void MainWindow::editSequences()
{
    // Откроем отдельное окно‑диалог с PromptSequenceEditor
    // Пользователь выбирает файл из списка, редактирует, сохраняет.
    // Для простоты покажем только один набор (первый в списке) – можно доработать.
    if (m_seqMgr.sequences().isEmpty()) {
        QMessageBox::information(this, "Нет наборов", "В папке нет ни одного *.seq.json");
        return;
    }

    qDebug()<< "m_seqMgr.root() = "<< m_seqMgr.root();


    // Выберем первый (можно добавить отдельный QComboBox)
    QString filePath = m_seqMgr.fileList().first();
    PromptSequence seq = m_seqMgr.sequences().value(filePath);

    // Окно‑редактор
    QDialog dlg(this);
    dlg.setWindowTitle("Редактор последовательности");
    dlg.resize(720, 540);
    auto *lay = new QVBoxLayout(&dlg);
    auto *editor = new PromptSequenceEditor(&dlg);
    editor->setSequence(seq);
    lay->addWidget(editor);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel, &dlg);
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, [&]{
        // Сохраняем
        PromptSequence edited = editor->sequence();
        if (m_seqMgr.save(edited, filePath))
            QMessageBox::information(&dlg, "Сохранено", "Последовательность сохранена.");
        else
            QMessageBox::warning(&dlg, "Ошибка", "Не удалось сохранить файл.");
        dlg.accept();
    });
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    dlg.exec();

    // После закрытия – обновляем кеш
    m_seqMgr.refresh();
    ensureDefaultSequence();
}

void MainWindow::runSequenceDialog()
{
    // Обновляем список (на случай, если пользователь добавил файлы вручную)
    m_seqMgr.refresh();
    ensureDefaultSequence();

    RunSequenceDialog dlg(m_seqMgr.sequences(), this);
    if (dlg.exec() != QDialog::Accepted) return;

    QString path = dlg.selectedFilePath();
    if (!m_seqMgr.sequences().contains(path)) {
        QMessageBox::warning(this, "Ошибка", "Файл последовательности не найден.");
        return;
    }

    m_currentSeq = m_seqMgr.sequences().value(path);
    m_currentStepIdx = 0;
    m_sequenceMode = true;
    runNextStep();   // сразу стартуем первый шаг
}

void MainWindow::runNextStep()
{
    if (!m_sequenceMode) return;
    if (m_currentStepIdx < 0 ||
            m_currentStepIdx >= m_currentSeq.steps().size()) {
        // конец последовательности
        QMessageBox::information(this, "Готово",
                                 "Последовательность выполнена полностью.");
        m_sequenceMode = false;
        return;
    }

    const PromptStep &step = m_currentSeq.steps()[m_currentStepIdx];
    QString resolved = PromptResolver::resolve(step.tmpl(), step, this);

    // Логируем, показываем пользователю
    log(QString("\n--- Шаг %1 ------------------------------------------------")
        .arg(m_currentStepIdx+1));
    log(resolved);

    // Отправляем запрос так же, как обычный ввод пользователя
    m_streamBuf.clear();
    m_model.startStream(resolved);
    // В истории сохраняем только запрос (не пользовательский текст)
    m_history.push_back(qMakePair(QString("system"), resolved));
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
        return;
    }

    m_history.push_back(qMakePair(QString("assistant"), QString(fullResp)));

    /* 4. Парсим команды */
    QVector<Command*> cmds = CommandParser::parse(jsonResp);

    /* ... остальная часть функции остаётся без изменений ... */
    //m_history.push_back(qMakePair(QString("<assistant>"), QString(fullResp)));

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


    // После выполнения всех подтверждённых команд:
    rescan(); // обновляем UI

    // Если мы в режиме последовательности – переходим к следующему шагу
    if (m_sequenceMode) {
        ++m_currentStepIdx;
        QTimer::singleShot(200, this, &MainWindow::runNextStep); // небольшая пауза
    }


    qDeleteAll(toRun);
}

void MainWindow::generateDescriptions()
{
    DescriptionGenerator generator(&m_model, &m_dev_manager, this);

    // 1. Перебираем файлы проекта
    QStringList fileList;
    QDirIterator it(m_projectRoot, QDir::Files, QDirIterator::Subdirectories);
    const QString devAssPath = m_dev_manager.devAssDir().absolutePath();

    while (it.hasNext()) {
        const QString srcFilePath = it.next();
        if (srcFilePath.startsWith(devAssPath))
            continue; // игнорируем DevAss

        FileDescription* fd = m_dev_manager.findFileDescription(srcFilePath);
        QDateTime fileMod = QFileInfo(srcFilePath).lastModified();

        if (fd && fd->isDescriptionUpToDate && fileMod <= fd->lastModTime)
            continue; // уже актуально

        fileList << srcFilePath;
    }

    if (fileList.isEmpty()) {
        QMessageBox::information(this, "Готово",
                                 "В проекте нет файлов, требующих описания.");
        return;
    }

    // 2. Генерация с прогресс‑диалогом
    QProgressDialog prog("Генерация описаний…", "Отмена",
                         0, fileList.size(), this);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(0);

    int processed = 0, failed = 0;
    for (const QString &srcFilePath : fileList) {
        if (prog.wasCanceled()) break;

        QString descr = generator.generateAndSave(srcFilePath);
        if (descr.isEmpty()){
            ++failed;
        }
        else{
            // 1. Относительный путь к исходному файлу
            QString relPath = QDir(m_projectRoot).relativeFilePath(srcFilePath);

            // 2. Путь к файлу‑дескриптору *.dadescr
            QString descrPath = m_dev_manager.devAssDir().filePath(relPath + ".dadescr");

            // 3. Имя файла дескриптора
            QString descrFileName = QFileInfo(descrPath).fileName();

            // 4. Логируем
            log(QString("✓ Описан %1 → %2").arg(relPath).arg(descrFileName));
        }
        ++processed;
        prog.setValue(processed);
    }

    prog.setValue(fileList.size());
    log(QString("Генерация завершена: %1 успешно, %2 ошибок.")
        .arg(processed - failed).arg(failed));

    rescan();
}

void MainWindow::log(const QString& m){ m_log->appendPlainText(m); }

// <‑‑ Добавлен новый слот для обработки текста от Vosk
void MainWindow::onVoskText(const QString& text){
    log("Vosk recognized: " + text);
    sendUserMessage(text);
}

/* ────────────────────────────────────────────────────── */
void MainWindow::ensureDefaultSequence()
{
    // Путь к файлу default.seq.json
    QString defaultFile = QDir(m_seqMgr.root()).filePath("default.seq.json");

    if (QFile::exists(defaultFile))
        return;        // уже есть – ничего не делаем

    // Создаём объект‑шаблон
    PromptSequence seq = defaultSequenceTemplate();

    // Сохраняем
    if (!m_seqMgr.save(seq, defaultFile))
        qWarning() << "Не удалось создать файл default.seq.json";
}


/* ────────────────────────────────────────────────────── */
PromptSequence MainWindow::defaultSequenceTemplate()
{
    // Шаг 1 – показать базовую информацию о проекте
    PromptStep step1;
    step1.setTemplate(
        "Кратко о проекте:\n"
        "  Корневой путь: {{PROJECT_ROOT}}\n"
        "  Список файлов: \n{{FILE_LIST}}\n"
        "  Доступные команды:\n{{SUPPORTED_COMMANDS}}\n"
        "  Введите запрос для следующего шага:"
    );
    // Разрешённые команды: все, которые умеет система
    QStringList allNames;
    for (Command* c : CommandRegistry::all()) {
        allNames << c->name();
        delete c;
    }
    step1.setAllowedCommands(allNames);

    // Шаг 2 – открываем (и показываем) какой‑то примерный файл, чтобы пользователь видел, как работает OPEN_FILE
    PromptStep step2;
    step2.setTemplate(
        "Откройте файл config.json и выведите его содержимое.\n"
        "Только команда OPEN_FILE доступна сейчас."
    );
    step2.setAllowedCommands(QStringList{"OPEN_FILE"});

    // Шаг 3 – генерируем README.md
    PromptStep step3;
    step3.setTemplate(
        "Сгенерируйте README.md, в котором будет список всех файлов проекта.\n"
        "Доступна команда CREATE_FILE."
    );
    step3.setAllowedCommands(QStringList{"CREATE_FILE"});

    PromptSequence seq("Default");
    seq.setDescription("Базовый набор, позволяющий быстро начать работать с любым проектом.");
    seq.addStep(step1);
    seq.addStep(step2);
    seq.addStep(step3);

    return seq;
}

