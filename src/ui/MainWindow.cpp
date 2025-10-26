#include "MainWindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>

#include <QStatusBar>
#include <QMessageBox>
#include <QJsonDocument>
#include "../ui/ChatDialog.h"
#include "../ui/ModelsDialog.h"
#include "../ui/OllamaSettingsDialog.h"
#include "../utils/AppConfig.h"
#include "../ui/PromptEditorDialog.h"
#include "../ui/ChangesDialog.h"
#include <QDir>
#include <QInputDialog>
#include <QRegularExpression>
#include <QFileInfo>
#include <algorithm>
#include "../command/EditInRangeCommand.h"
#include "../command/RunShellCommand.h"
#include "../command/RunPythonCommand.h"

static QList<QByteArray> splitConcatenatedJson_fromText(const QString& text){
    QByteArray raw = text.toUtf8();
    QList<QByteArray> parts;
    const char* s = raw.constData();
    int n = raw.size();
    bool in_str=false, esc=false;
    int depth=0, start=-1;
    for (int i=0;i<n;i++){
        char c = s[i];
        if (in_str){
            if (esc){ esc=false; continue; }
            if (c=='\\') { esc=true; continue; }
            if (c=='"') { in_str=false; continue; }
            continue;
        }
        if (c=='"'){ in_str=true; continue; }
        if (c=='{' || c=='['){
            if (depth==0) start=i;
            depth++;
        } else if (c=='}' || c==']'){
            depth--;
            if (depth==0 && start!=-1){
                parts.append(raw.mid(start, i-start+1));
                start=-1;
            }
        }
    }
    return parts;
}

static QString normalizeContextPath(const QString& path, const QString& projectRoot)
{
    if (path.trimmed().isEmpty())
        return QString();
    QFileInfo info(path);
    if (info.isAbsolute())
        return QDir::cleanPath(info.absoluteFilePath());
    return QDir(projectRoot).absoluteFilePath(path);
}

static QString displayContextPath(const QString& normalizedPath, const QString& projectRoot)
{
    QDir root(projectRoot);
    QString rel = root.relativeFilePath(normalizedPath);
    if (!rel.startsWith(".."))
        return QDir::toNativeSeparators(rel);
    return QDir::toNativeSeparators(normalizedPath);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // --------  1. Заголовок и корневой виджет ----------
    setWindowTitle("DevAssistantQt");

    // --------  2. IDE‑виджет (верхняя часть) ----------
    ide_widget = new IDEWidget("ideSplitter", Qt::Horizontal, this);          // <-- родитель `this`

    // --------  3. Вспомогательные виджеты (нижняя часть) ----------
    m_rootEdit  = new MemoryLineEdit("projectRoot", this);

    m_graphWindow = new GraphWidget();
    browseBtn   = new StyledButton("Browse...", this);

    // 3.1   Верхняя строка (путь + Browse)
    QWidget * top_widget = new QWidget(this);
    auto top = new QHBoxLayout(top_widget);
    top->addWidget(m_rootEdit, 1);
    top->addWidget(browseBtn);

    // 3.2   Список файлов
    m_files = new QListWidget(this);

    QWidget *buttons_widget = new QWidget();
    // 3.3   Кнопки
    auto btns = new QHBoxLayout(buttons_widget);
    rescanBtn   = new StyledButton("Rescan", this);
    settingsBtn = new StyledButton("Ollama Settings", this);
    promptBtn   = new StyledButton("Edit Base Prompt", this);
    applyBtn    = new StyledButton("Apply changes", this);
    undoBtn     = new StyledButton("Undo", this);
    descBtn     = new StyledButton("Generate Descriptions", this);

    btns->addWidget(runSeqBtn  = new StyledButton(tr("Запустить последовательность"), this));
    btns->addWidget(editSeqBtn = new StyledButton(tr("Редактировать последовательности"), this));
    btns->addWidget(newSeqBtn  = new StyledButton("New sequence…", this));

    connect(newSeqBtn, &StyledButton::clicked, this, &MainWindow::createNewSequence);

    btns->addWidget(rescanBtn);
    btns->addWidget(settingsBtn);
    btns->addWidget(descBtn);


    // ---------- Добавляем кнопку «Show Graph» ----------
    graphBtn = new StyledButton("Show Graph", this);
    btns->addWidget(graphBtn);
    connect(graphBtn, &StyledButton::clicked, this,
            &MainWindow::openGraph);
    // -------------------------------------------------

    promptBtn->hide();
    applyBtn->hide();
    undoBtn->hide();

    // 3.4   Логи и чат
    m_log = new StyleTextEdit(this);
    m_log->setReadOnly(true);

    m_chat = new ChatDialog(&m_model, this);

    // 3.5   (Опционально) виджет распознавания голоса
    m_voskWidget = new VoskWidget(this);
    connect(m_voskWidget, &VoskWidget::voiceRecognized,
            this, &MainWindow::onVoskText);

    // --------  4. Верхняя часть layout (внутри rightPanel) ----------
    /*QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->addLayout(top);
    rightLayout->addWidget(m_files, 1);
    rightLayout->addLayout(btns);
    rightLayout->addWidget(m_log, 1);
    rightLayout->addWidget(m_chat, 1);*/


    // --------  5. Создаём вертикальный splitter ----------
    MemorySplitter *mainSplitter = new MemorySplitter("MainWindowsMainSplitter",Qt::Vertical, this);
    mainSplitter->addWidget(top_widget);
    mainSplitter->addWidget(ide_widget);   // верхняя часть
    //mainSplitter->addWidget(m_files);
    mainSplitter->addWidget(m_log);
    mainSplitter->addWidget(m_chat);
    mainSplitter->addWidget(buttons_widget);

    m_files->hide();



    //QWidget *rightPanel = new QWidget(this);
    //rightPanel->setLayout(rightLayout);    // нижняя часть
    //mainSplitter->addWidget(rightPanel);

    // Задаём начальные размеры (опционально)
    mainSplitter->setStretchFactor(0, 2);  // IDE
    mainSplitter->setStretchFactor(1, 1);  // UI справа

    setCentralWidget(mainSplitter);

    // --------  6. Соединяем сигналы / слоты ----------
    connect(browseBtn,   &StyledButton::clicked, this, &MainWindow::browse);
    connect(rescanBtn,   &StyledButton::clicked, this, &MainWindow::rescan);
    connect(settingsBtn, &StyledButton::clicked, this, &MainWindow::openSettings);
    connect(promptBtn,   &StyledButton::clicked, this, &MainWindow::editBasePrompt);
    connect(applyBtn,    &StyledButton::clicked, this, &MainWindow::applyChanges);
    connect(undoBtn,     &StyledButton::clicked, this, &MainWindow::undoLast);

    // Chat + модель
    connect(m_chat, &ChatDialog::sendMessage, this, &MainWindow::sendUserMessage);
    connect(&m_model, &OllamaAdapter::streamChunk, this, &MainWindow::onModelChunk);
    connect(&m_model, &OllamaAdapter::streamFinished, this, &MainWindow::handleAssistantFinished);
    connect(&m_model, &OllamaAdapter::streamError,
            this, [this](const QString &e){ log("Model error: " + e); });

    connect(runSeqBtn, &StyledButton::clicked, this, &MainWindow::runSequenceDialog);
    connect(editSeqBtn, &StyledButton::clicked, this, &MainWindow::editSequences);

    connect(descBtn, &StyledButton::clicked, this, &MainWindow::generateDescriptions);

    // Состояния
    statusBar()->showMessage(QString("Model: %1").arg(m_model.selectedModel()));

    m_projectRoot = m_rootEdit->text();



    // Папка, где будем хранить наборы
    QString seqRoot = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                        .append("/DevelopersAssistant/PromptSequences");

    // Создаём каталог, если его нет
    QDir seqDir(seqRoot);
    if (!seqDir.exists())
        seqDir.mkpath(".");

    m_seqMgr.setRoot(seqRoot);
    m_seqMgr.refresh();
    ensureDefaultSequence();

    connect(m_chat, &ChatDialog::signal_updatelastUserMessage, this, &MainWindow::slot_updatelastUserMessage);


    connect(ide_widget, &IDEWidget::signal_runSequenceDialog, this, &MainWindow::runSequenceDialog);

    connect(ide_widget, &IDEWidget::signal_updatelastUserMessage,this, &MainWindow::slot_updatelastUserMessage);

    connect(this, &MainWindow::missingFilesUpdated,
                ide_widget, &IDEWidget::updateMissingFiles);


    reOpenRootDir();

    mainSplitter->loadState();

    //rescan();

    // ------------ Управление последовательностями ----------------
    // … (тот же код, что был ранее)
}


void MainWindow::openGraph()
{
    m_graphWindow->show();
    m_graphWindow->raise();
    m_graphWindow->activateWindow();
}

void MainWindow::browse(){
    QString d = QFileDialog::getExistingDirectory(this,"Select project folder", m_projectRoot);
    if (d.isEmpty()) return;

    m_projectRoot = d;
    reOpenRootDir();
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

    QSet<QString> needRed_set;
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

        if (needRed){
            needRed_set.insert(absPath);
            item->setBackgroundColor(Qt::red);
        }
        else
            item->setBackgroundColor(Qt::green);   // можно оставить default

        /* Храним абсолютный путь в UserRole */
        item->setData(Qt::UserRole, absPath);
    }

    emit missingFilesUpdated(needRed_set);



    log("Rescanned project.");
}

void MainWindow::openSettings(){
    OllamaSettingsDialog d(this);
    if (d.exec()==QDialog::Accepted) {
        m_model.setEndpoint(QUrl(d.endpoint()));
        QVariantMap opts = d.params();
        if (!m_model.selectedModel().isEmpty()) m_model.configure(m_model.selectedModel(), opts);
        // persist
        //{ AppConfig cfg; cfg.load(); cfg.params = opts; cfg.save(); }
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

        s += " - " + QDir::toNativeSeparators(abs) + '\n';
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
    m_lastUserMessage = text;
    qDebug() << "[sendUserMessage] updated last user message =" << m_lastUserMessage;
    runSequenceDialog();
/*
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
*/

}

void MainWindow::onModelChunk(const QString& s){
    m_streamBuf += s;
    if (m_chat) m_chat->appendAssistantChunk(s);
}

QByteArray MainWindow::extractJsonPart(const QByteArray& txt) const
{
    QList<QByteArray> parts = splitConcatenatedJson_fromText(QString::fromUtf8(txt));
    if (parts.isEmpty())
        return QByteArray();

    if (parts.size() == 1) {
        const QByteArray trimmed = parts.first().trimmed();
        if (!trimmed.isEmpty() && trimmed.startsWith('['))
            return trimmed;
    }

    QByteArray joined = "[";
    for (int i = 0; i < parts.size(); ++i) {
        if (i)
            joined += ',';
        joined += parts.at(i);
    }
    joined += "]";
    return joined;
}

void MainWindow::editSequences()
{
    // 1. Проверяем, что есть наборы
    if (m_seqMgr.sequences().isEmpty()) {
        QMessageBox::information(this, tr("Нет наборов"),
                                 tr("В папке нет ни одного *.seq.json"));
        return;
    }

    // 2. Формируем список названий файлов (без пути)
    QStringList names;
    QMap<QString, QString> pathByName;      // имя → путь
    for (const QString &filePath : m_seqMgr.fileList()) {
        QString name = QFileInfo(filePath).fileName();
        names << name;
        pathByName.insert(name, filePath);
    }

    // 3. Попросим пользователя выбрать файл
    bool ok = false;
    QString selectedName = QInputDialog::getItem(
                this, tr("Выберите последовательность"),
                tr("Файл:"), names, 0, false, &ok);

    if (!ok || selectedName.isEmpty())
        return;          // пользователь отменил

    QString filePath = pathByName.value(selectedName);

    // 4. Загружаем выбранную последовательность
    PromptSequence seq = m_seqMgr.sequences().value(filePath);

    // 5. Откроем диалог‑редактор
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Редактор последовательности") + ": " + selectedName);
    dlg.resize(720, 540);

    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    PromptSequenceEditor *editor = new PromptSequenceEditor(&dlg);
    editor->setSequence(seq);
    lay->addWidget(editor);

    QDialogButtonBox *bb = new QDialogButtonBox(
                QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    lay->addWidget(bb);

    // 6. Сохранение
    connect(bb, &QDialogButtonBox::accepted, &dlg, [=, &dlg]{
        PromptSequence edited = editor->sequence();
        if (m_seqMgr.save(edited, filePath)) {
            QMessageBox::information(&dlg, tr("Сохранено"),
                                     tr("Последовательность сохранена."));
        } else {
            QMessageBox::warning(&dlg, tr("Ошибка"),
                                 tr("Не удалось сохранить файл."));
        }
        dlg.accept();
    });

    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    dlg.exec();

    // 7. Обновляем кеш после редактирования
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
    PromptResolver::ResolvedPrompt resolved = PromptResolver::resolve(step.tmpl(), step, this);
    qDebug() << "[runNextStep] systemPrompt snippet:" << resolved.systemPrompt.left(200);
    qDebug() << "[runNextStep] userPrompt:" << resolved.userPrompt;

    // РћС†РµРЅРєР° С‚РѕРєРµРЅРѕРІ РґР»СЏ РєРѕРЅС‚РµРєСЃС‚Р° С€Р°РіР°
    {
        int tokens = m_model.estimateTokens(resolved.systemPrompt + "\n" + resolved.userPrompt);
        log(QString("[РЁР°Рі %1] РљРѕРЅС‚РµРєСЃС‚ в‰€ %2 С‚РѕРєРµРЅРѕРІ").arg(m_currentStepIdx+1).arg(tokens));
    }
    // Р›РѕРіРёСЂСѓРµРј, РїРѕРєР°Р·С‹РІР°РµРј РїРѕР»СЉР·РѕРІР°С‚РµР»СЉ
    log(QString("\n--- РЁР°Рі %1 ------------------------------------------------")
        .arg(m_currentStepIdx+1));
    log(resolved.systemPrompt);
    if (!resolved.userPrompt.isEmpty())
        log(QString("USER_QUERY:\n%1").arg(resolved.userPrompt));

    // РћС‚РїСЂР°РІР»СЏРµРј Р·Р°РїСЂРѕСЃ С‚Р°Рє Р¶Рµ, РєР°Рє РѕР±С‹С‡РЅС‹Р№ РІРІРѕРґ РїРѕР»СЉР·РѕРІР°С‚РµР»СЉ
    m_streamBuf.clear();
    m_model.startStreamWithSystem(resolved.userPrompt, resolved.systemPrompt);
    // В истории сохраняем только запрос (не пользовательский текст)
    //m_history.push_back(qMakePair(QString("system"), resolved));
}
/*
void MainWindow::handleAssistantFinished()
{
    // 1. Ответ LLM – полный текст (включая «лишние» строки)
    QByteArray fullResp = m_streamBuf.toUtf8();

    // 2. Выбираем только JSON‑часть
    QByteArray jsonResp = extractJsonPart(fullResp);

    // 3. Проверяем, что JSON найден
    if (jsonResp.isEmpty()) {
        log("Assistant ответил, но не содержит JSON-массив. Команды не применяются.");
        log(QString("Сырой ответ модели:\n%1").arg(QString::fromUtf8(fullResp)));
        const QJsonObject lastBody = m_model.lastRequestBody();
        if (!lastBody.isEmpty()) {
            log(QString("Последний запрос к Ollama:\n%1")
                    .arg(QString::fromUtf8(QJsonDocument(lastBody).toJson(QJsonDocument::Indented))));
        }
        //return;
    }

    m_history.push_back(qMakePair(QString("assistant"), QString(fullResp)));

    // 4. Парсим команды
    QVector<Command*> cmds = CommandParser::parse(jsonResp);

    // ... остальная часть функции остаётся без изменений ...
    //m_history.push_back(qMakePair(QString("<assistant>"), QString(fullResp)));

    if (cmds.isEmpty()) {
        log("LLM не вернул ни одной команды");
        return;
    }

    // 5. Подтверждение пользователем
    CommandConfirmDialog dlg(cmds.toList(), this);
    if (dlg.exec() != QDialog::Accepted) {
        log("Пользователь отклонил команды");
        qDeleteAll(cmds);
        return;
    }
    QList<Command*> toRun = dlg.confirmed();

    // 6. Обработка специальных команд (без изменений на диске)
    for (Command* c : toRun) {
        if (c->name()=="ASK_USER") {
            QString q = c->params().value("question").toString();
            bool ok=false; QString ans = QInputDialog::getText(this, tr("Ответьте на вопрос нейросети"), q, QLineEdit::Normal, QString(), &ok);
            if (ok) { m_history.push_back(qMakePair(QString("user"), ans)); }
        } else if (c->name()=="OPEN_FILE_REGION" || c->name()=="OPEN_FILE") {
            QJsonObject p = c->params(); QString path = p.value("path").toString(); QString abs = QDir(m_projectRoot).filePath(path);
            QFile f(abs); if (f.open(QIODevice::ReadOnly)) {
                QString txt = QString::fromUtf8(f.readAll()); f.close();
                QString snippet;
                if (c->name()=="OPEN_FILE_REGION") {
                    int sLine = p.value("startLine").toInt(-1); int eLine = p.value("endLine").toInt(-1);
                    QString region = p.value("region").toString();
                    if (sLine>=1 && eLine>=sLine) {
                        QStringList lines = txt.split("\n");
                        sLine = qMax(1, sLine); eLine = qMin(eLine, lines.size());
                        snippet = lines.mid(sLine-1, eLine-sLine+1).join("\n");
                    } else if (!region.isEmpty()) {
                        QRegularExpression re(region, QRegularExpression::DotMatchesEverythingOption|QRegularExpression::MultilineOption);
                        auto m = re.match(txt);
                        if (m.hasMatch()) snippet = m.captured(0);
                    } else {
                        snippet = txt;
                    }
                } else { snippet = txt; }
                if (!snippet.isEmpty()) {
                    m_contextContent += QString("\n==== %1 ====%2\n%3\n").arg(path).arg(c->name()=="OPEN_FILE_REGION"?" (region)":"").arg(snippet);
                }
            }
        }
    }

    // 6. Применяем команды

    // Сводка для PREVIOUS_STEPS
    QStringList appliedSummary;
    for (Command* c : toRun) {
        QString one = c->name();
        QJsonObject p = c->params();
        if (p.contains("path")) one += QString(" path=%1").arg(p.value("path").toString());
        if (p.contains("from") && p.contains("to")) one += QString(" %1→%2").arg(p.value("from").toString(), p.value("to").toString());
        appliedSummary << one;
    }
    if (!appliedSummary.isEmpty()) {
        m_prevSteps << QString("Шаг %1: %2").arg(m_currentStepIdx+1).arg(appliedSummary.join("; "));
    }

    QString err;
    for (Command* c : toRun) {
        if (!c->apply(&m_dev_manager, &err)) {
            QMessageBox::warning(this, "Ошибка выполнения", err);
            break;
        }

    }

    // 7. После применения обновляем список
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
*/

void MainWindow::handleAssistantFinished()
{
    QByteArray fullResp = m_streamBuf.toUtf8();
    QByteArray jsonResp = extractJsonPart(fullResp);
    const bool usedRaw = jsonResp.isEmpty();
    const QByteArray payload = usedRaw ? fullResp : jsonResp;
    if (usedRaw) {
        appendConsoleContext("LLM RAW OUTPUT", QString::fromUtf8(fullResp), QString(), -1);
        log("LLM ответ сохранён как контекст: не удалось извлечь JSON. Пытаемся распарсить сырой ответ.");
    }

    m_history.push_back(qMakePair(QString("assistant"), QString::fromUtf8(payload)));
    m_lastAssistantMessage = QString::fromUtf8(payload);

    QVector<Command*> cmds = CommandParser::parse(payload);
    if (cmds.isEmpty()) {
        if (!usedRaw)
            appendConsoleContext("LLM RAW OUTPUT", QString::fromUtf8(payload), QString(), -1);
        log("LLM не вернул команд. Сырой ответ добавлен в контекст.");
        retryCurrentSequenceStep();
        return;
    }

    CommandConfirmDialog dlg(cmds.toList(), this);
    if (dlg.exec() != QDialog::Accepted) {
        log("Пользователь отклонил команды");
        qDeleteAll(cmds);
        return;
    }
    QList<Command*> toRun = dlg.confirmed();

    for (Command* c : toRun) {
        if (c->name() == "ASK_USER") {
            const QString question = c->params().value("question").toString();
            bool ok = false;
            const QString ans = QInputDialog::getText(this,
                                                      tr("Ответьте на вопрос нейросети"),
                                                      question,
                                                      QLineEdit::Normal,
                                                      QString(),
                                                      &ok);
            if (ok)
                m_history.push_back(qMakePair(QString("user"), ans));
        } else if (c->name() == "OPEN_FILE_REGION" || c->name() == "OPEN_FILE") {
            handleOpenCommand(c);
        }
    }

    QStringList appliedSummary;
    for (Command* c : toRun) {
        QString one = c->name();
        QJsonObject p = c->params();
        if (p.contains("path"))
            one += QString(" path=%1").arg(p.value("path").toString());
        if (p.contains("from") && p.contains("to"))
            one += QString(" %1→%2").arg(p.value("from").toString(), p.value("to").toString());
        appliedSummary << one;
    }
    if (!appliedSummary.isEmpty()) {
        m_prevSteps << QString("Шаг %1: %2").arg(m_currentStepIdx + 1).arg(appliedSummary.join("; "));
    }

    QString err;
    auto logConsoleResult = [&](Command* cmd){
        QString title;
        QString stdoutText;
        QString stderrText;
        int exitCode = 0;

        if (auto py = dynamic_cast<RunPythonCommand*>(cmd)) {
            QJsonObject p = py->params();
            QString script = p.value("scriptPath").toString();
            if (!script.isEmpty() && !QFileInfo(script).isAbsolute())
                script = QDir(m_projectRoot).filePath(script);
            if (script.isEmpty())
                script = p.value("path").toString();
            title = QString("RUN_PYTHON %1").arg(script.isEmpty() ? QStringLiteral("<inline>") : script);
            stdoutText = py->stdoutText();
            stderrText = py->stderrText();
            exitCode = py->exitCode();
        } else if (auto sh = dynamic_cast<RunShellCommand*>(cmd)) {
            QString commandText = sh->params().value("command").toString();
            title = QString("RUN_SHELL %1").arg(commandText);
            stdoutText = sh->stdoutText();
            stderrText = sh->stderrText();
            exitCode = sh->exitCode();
        }

        if (!title.isEmpty())
            appendConsoleContext(title.trimmed(), stdoutText, stderrText, exitCode);
    };

    QList<Command*> pendingEdits;
    bool applyFailed = false;
    for (Command* c : toRun) {
        const QString name = c->name();
        if (name == "ASK_USER" || name == "OPEN_FILE" || name == "OPEN_FILE_REGION")
            continue;
        if (name == "EDIT_IN_RANGE") {
            pendingEdits << c;
            continue;
        }
        if (!c->apply(&m_dev_manager, &err)) {
            logConsoleResult(c);
            QMessageBox::warning(this, tr("Ошибка выполнения"), err);
            applyFailed = true;
            break;
        }
        handlePostApplyContextCleanup(c);
        logConsoleResult(c);
    }

    if (!applyFailed && !pendingEdits.isEmpty()) {
        if (!applyGroupedEditInRange(pendingEdits, &err)) {
            QMessageBox::warning(this, tr("Ошибка выполнения"), err);
        } else {
            for (Command* c : pendingEdits)
                handlePostApplyContextCleanup(c);
        }
    }

    rescan();

    if (m_sequenceMode) {
        const PromptStep &step = m_currentSeq.steps()[m_currentStepIdx];
        if (step.loop()) {
            QTimer::singleShot(200, this, &MainWindow::runNextStep);
        } else {
            ++m_currentStepIdx;
            QTimer::singleShot(200, this, &MainWindow::runNextStep);
        }
    }

    qDeleteAll(toRun);
}

void MainWindow::handleOpenCommand(Command* cmd)
{
    const QJsonObject params = cmd->params();
    const QString rawPath = params.value("path").toString();
    if (rawPath.isEmpty())
        return;

    const QString normalized = normalizeContextPath(rawPath, m_projectRoot);
    QFile file(normalized);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    const QString text = QString::fromUtf8(file.readAll());
    file.close();

    QStringList allLines = text.split('\n', QString::KeepEmptyParts);
    if (text.endsWith('\n') && !allLines.isEmpty())
        allLines.removeLast();
    if (cmd->name() == "OPEN_FILE") {
        setFullFileContext(normalized, allLines);
        return;
    }

    int startLine = params.value("startLine").toInt(-1);
    int endLine = params.value("endLine").toInt(-1);
    QStringList snippet;

    if (startLine >= 1 && endLine >= startLine) {
        if (startLine > allLines.size())
            return;
        startLine = qMax(1, startLine);
        endLine = qMin(endLine, allLines.size());
        snippet = allLines.mid(startLine - 1, endLine - startLine + 1);
    } else {
        const QString region = params.value("region").toString();
        if (region.isEmpty())
            return;
        QRegularExpression re(region, QRegularExpression::DotMatchesEverythingOption
                                        | QRegularExpression::MultilineOption);
        auto match = re.match(text);
        if (!match.hasMatch())
            return;
        const int startIndex = match.capturedStart();
        startLine = text.left(startIndex).count('\n') + 1;
        const QString captured = match.captured(0);
        snippet = captured.split('\n', QString::KeepEmptyParts);
        if (captured.endsWith('\n') && !snippet.isEmpty())
            snippet.removeLast();
    }

    if (!snippet.isEmpty())
        addRegionContext(normalized, startLine, snippet);
}

void MainWindow::rebuildContextCache()
{
    QStringList blocks;
    for (auto it = m_contextSlices.cbegin(); it != m_contextSlices.cend(); ++it) {
        const FileContextSlice& slice = it.value();
        if (slice.mode == FileContextSlice::None)
            continue;

        QString block = QString("==== %1 ====")
                .arg(displayContextPath(it.key(), m_projectRoot));
        if (slice.mode == FileContextSlice::Ranges)
            block += " (selected lines)";
        block += "\n";

        if (slice.mode == FileContextSlice::FullFile) {
            block += formatNumberedLines(slice.fullFileLines, 1);
        } else {
            for (const ContextRange& range : slice.regions) {
                block += QString("[lines %1-%2]\n").arg(range.startLine).arg(range.endLine);
                block += formatNumberedLines(range.lines, range.startLine);
                block += "\n";
            }
            block = block.trimmed();
        }
        blocks << block.trimmed();
    }
    m_contextContent = blocks.join("\n\n");
    if (!m_consoleLogs.isEmpty()) {
        if (!m_contextContent.isEmpty())
            m_contextContent += "\n\n";
        m_contextContent += m_consoleLogs.join("\n\n");
    }
}

QString MainWindow::formatNumberedLines(const QStringList& lines, int startLine) const
{
    if (lines.isEmpty())
        return QString();
    const int width = QString::number(startLine + lines.size() - 1).size();
    QStringList numbered;
    for (int i = 0; i < lines.size(); ++i) {
        const int lineNo = startLine + i;
        numbered << QString("%1| %2")
                        .arg(lineNo, width, 10, QChar('0'))
                        .arg(lines.at(i));
    }
    return numbered.join("\n");
}

void MainWindow::setFullFileContext(const QString& path, const QStringList& lines)
{
    const QString normalized = normalizeContextPath(path, m_projectRoot);
    if (normalized.isEmpty())
        return;
    FileContextSlice& slice = m_contextSlices[normalized];
    slice.mode = FileContextSlice::FullFile;
    slice.fullFileLines = lines;
    slice.regions.clear();
    rebuildContextCache();
}

void MainWindow::addRegionContext(const QString& path, int startLine, const QStringList& lines)
{
    const QString normalized = normalizeContextPath(path, m_projectRoot);
    if (normalized.isEmpty() || lines.isEmpty())
        return;

    FileContextSlice& slice = m_contextSlices[normalized];
    slice.mode = FileContextSlice::Ranges;
    slice.fullFileLines.clear();

    ContextRange incoming;
    incoming.startLine = startLine;
    incoming.lines = lines;
    incoming.endLine = startLine + lines.size() - 1;

    bool replaced = false;
    for (ContextRange& existing : slice.regions) {
        if (existing.startLine == incoming.startLine && existing.endLine == incoming.endLine) {
            existing.lines = lines;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        slice.regions.append(incoming);
        std::sort(slice.regions.begin(), slice.regions.end(),
                  [](const ContextRange& a, const ContextRange& b){
                      return a.startLine < b.startLine;
                  });
    }
    rebuildContextCache();
}

void MainWindow::clearContextForPath(const QString& path)
{
    const QString normalized = normalizeContextPath(path, m_projectRoot);
    if (normalized.isEmpty())
        return;
    if (m_contextSlices.remove(normalized) > 0)
        rebuildContextCache();
}

bool MainWindow::applyGroupedEditInRange(const QList<Command*>& commands, QString* err)
{
    QMap<QString, QList<EditInRangeCommand*>> grouped;
    for (Command* c : commands) {
        auto edit = dynamic_cast<EditInRangeCommand*>(c);
        if (!edit)
            continue;
        const QString normalized = normalizeContextPath(edit->path(), m_projectRoot);
        grouped[normalized].append(edit);
    }

    for (auto it = grouped.begin(); it != grouped.end(); ++it) {
        if (!EditInRangeCommand::applyBatch(it.value(), &m_dev_manager, err))
            return false;
        clearContextForPath(it.key());
    }
    return true;
}

void MainWindow::handlePostApplyContextCleanup(Command* cmd)
{
    const QString name = cmd->name();
    const QJsonObject p = cmd->params();
    auto clear = [&](const QString& key) {
        if (!key.isEmpty())
            clearContextForPath(key);
    };

    if (name == "EDIT_IN_RANGE" || name == "REPLACE_IN_FILE"
        || name == "DELETE_FILE" || name == "CREATE_FILE") {
        clear(p.value("path").toString());
        return;
    }

    if (name == "RENAME_FILE") {
        clear(p.value("oldPath").toString());
        clear(p.value("newPath").toString());
    }
}

void MainWindow::appendConsoleContext(const QString& title,
                                      const QString& stdOut,
                                      const QString& stdErr,
                                      int exitCode)
{
    QString block = QString("==== %1 ====\n").arg(title.isEmpty() ? "Console" : title);
    if (!stdOut.trimmed().isEmpty()) {
        block += "[stdout]\n" + stdOut.trimmed() + "\n";
    }
    if (!stdErr.trimmed().isEmpty()) {
        block += "[stderr]\n" + stdErr.trimmed() + "\n";
    }
    block += QString("exitCode=%1").arg(exitCode);
    m_consoleLogs << block.trimmed();
    const int maxLogs = 5;
    while (m_consoleLogs.size() > maxLogs)
        m_consoleLogs.removeFirst();
    rebuildContextCache();
}

void MainWindow::retryCurrentSequenceStep()
{
    if (!m_sequenceMode)
        return;
    QTimer::singleShot(200, this, &MainWindow::runNextStep);
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

void MainWindow::log(const QString& m){ m_log->append(m); }

// <‑‑ Добавлен новый слот для обработки текста от Vosk
void MainWindow::onVoskText(const QString& text){
    log("Vosk recognized: " + text);
    sendUserMessage(text);
}

void MainWindow::slot_updatelastUserMessage(const QString &lastUserMessage)
{
    if (lastUserMessage.trimmed().isEmpty())
        return;
    m_lastUserMessage = lastUserMessage;
    qDebug()<<"m_lastUserMessage = "<<m_lastUserMessage;
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

void MainWindow::reOpenRootDir(){
    if(m_projectRoot.isEmpty()){
        return;
    }


    ide_widget->openProjectbyPath(m_projectRoot);

    m_rootEdit->setText(m_projectRoot);
    m_dev_manager.setProjectRoot(m_projectRoot);
    m_promptMgr.setProjectRoot(m_projectRoot);

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


/* ────────────────────────────────────────────────────── */
PromptSequence MainWindow::defaultSequenceTemplate()
{
    // Step 1: Clarify the task if needed
    PromptStep s1;
    s1.setTemplate(
        "Ты — ИИ-помощник по рефакторингу и правкам кода.\n"
        "Контекст проекта:\n{{PROJECT_ROOT}}"
        "Вот список файлов:\n{{FILE_LIST}}\n"
        "\n{{HISTORY}}\n"
        "Если задача пользователя не до конца ясна — задай до 3 уточняющих вопросов, используя команду ASK_USER (один JSON на вопрос)."
        "Доступные команды: {{SUPPORTED_COMMANDS}}"
        "Затем опиши краткий план действий."
    );
    s1.setAllowedCommands(QStringList{ "ASK_USER", "OPEN_FILE_REGION", "OPEN_FILE" });

    // Step 2: Gather minimal relevant context (snippets only)
    PromptStep s2;
    s2.setTemplate(
        "На основе запроса и плана выбери минимальные необходимые участки кода."
        "Используй OPEN_FILE_REGION (желательно c startLine/endLine или регуляркой) для точечных фрагментов."
        "Если нужен целый файл (редко) — используй OPEN_FILE."
        "Выведи только команды."
    );
    s2.setAllowedCommands(QStringList{ "OPEN_FILE_REGION", "OPEN_FILE" });

    // Step 3: Propose concrete edits as commands (dry‑run)
    PromptStep s3;
    s3.setTemplate(
        "Сформируй набор правок."
        "• Для замены — REPLACE_IN_FILE"
        "• Для правки диапазона — EDIT_IN_RANGE"
        "• Для создания — CREATE_FILE"
        "• Для переименования/удаления — RENAME_FILE/DELETE_FILE"
        "Покажи только список JSON-команд без пояснений.\n"
        "Контекст:\n{{OPEN_FILES}}\n"
    );
    s3.setAllowedCommands(QStringList{ "REPLACE_IN_FILE", "EDIT_IN_RANGE", "CREATE_FILE", "RENAME_FILE", "DELETE_FILE" });

    // Step 4: If compilation hints are needed, ask short questions (optional)
    PromptStep s4;
    s4.setTemplate(
        "Если для точности правок не хватает 1-2 деталей — задай вопросы через ASK_USER, иначе верни пустой список."
    );
    s4.setAllowedCommands(QStringList{ "ASK_USER" });

    // Step 5: Finalize edits
    PromptStep s5;
    s5.setTemplate(
        "Выведи окончательные команды для применения изменений (повтори весь набор без пояснений)."
    );
    s5.setAllowedCommands(QStringList{ "REPLACE_IN_FILE", "EDIT_IN_RANGE", "CREATE_FILE", "RENAME_FILE", "DELETE_FILE" });

    PromptSequence seq("Default");
    seq.setDescription("Поэтапная последовательность правок: уточнение → сбор минимального контекста → черновик правок → уточнения → финальные команды.");
    seq.addStep(s1);
    seq.addStep(s2);
    seq.addStep(s3);
    seq.addStep(s4);
    seq.addStep(s5);
    return seq;
};







QString MainWindow::previousStepsText() const
{
    if (m_prevSteps.isEmpty()) return QString("Пока ничего не выполнено.");
    QString out;
    for (const QString& s : m_prevSteps) out += " - " + s + "\n";
    return out.trimmed();
}

QString MainWindow::fileDescriptionsText() const
{
    QString out;
    QDir dev = m_dev_manager.devAssDir();
    if (!dev.exists()) return out;

    // Iterate through all *.dadescr files and include their content with absolute target path
    QDirIterator it(dev.absolutePath(), QStringList()<<"*.dadescr", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()){
        QString descrPath = it.next();
        QFile f(descrPath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QString descr = QString::fromUtf8(f.readAll());
        f.close();

        // Derive original file path (without .dadescr) relative to project root
        QString rel = dev.relativeFilePath(descrPath);
        rel.chop(QString(".dadescr").size());
        QString absTarget = QDir(m_projectRoot).filePath(rel);

        out += "==== " + QDir::toNativeSeparators(absTarget) + " ====\n";
        out += descr.trimmed() + "\n\n";
    }
    return out.trimmed();
}

void MainWindow::createNewSequence()
{
    // Ask user for name
    bool ok=false;
    QString name = QInputDialog::getText(this, "New sequence", "Name:", QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    // Determine directory to save
    QString dir;
    if (!m_seqMgr.fileList().isEmpty()) {
        dir = QFileInfo(m_seqMgr.fileList().first()).absolutePath();
    } else {
        dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                .append("/DevelopersAssistant/PromptSequences");
        QDir().mkpath(dir);
        m_seqMgr.setRoot(dir);
    }
    QString filePath = QDir(dir).filePath(name.trimmed() + ".seq.json");

    // Create default template and save
    PromptSequence seq = defaultSequenceTemplate();
    seq.setName(name.trimmed());
    if (!m_seqMgr.save(seq, filePath)) {
        QMessageBox::warning(this, "Error", "Cannot save sequence to: " + filePath);
        return;
    }
    m_seqMgr.refresh();
    ensureDefaultSequence();
    QMessageBox::information(this, "OK", "Sequence created:\n" + filePath);
}
