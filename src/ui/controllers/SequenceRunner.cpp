#include "SequenceRunner.h"
#include "../MainWindow.h"
#include "../../adapters/OllamaAdapter.h"
#include "../../command/CommandParser.h"
#include "../../command/CommandRegistry.h"
#include "../../command/CommandDocs.h"
#include "../../command/CommandConfirmDialog.h"
#include "../../command/CommandPolicy.h"
#include "../../command/EditInRangeCommand.h"
#include "../../command/SearchWebCommand.h"
#include "../../command/SearchProjectCommand.h"
#include "../../utils/json_utils.h"
#include "../../core/PromptSequenceManager.h"
#include "../../core/PromptSequence.h"
#include "../../core/PromptResolver.h"
#include "../../core/DevAssManager.h"
#include "../../core/ContextManager.h"
#include "../../core/SolutionTemplateManager.h"
#include "../../ui/ResultDisplayWidget.h"
#include "../../ui/customWidgets/MemoryComboBox.h"
#include "../../ui/RunTemplateOrSequenceDialog.h"
#include "../../ui/controllers/ContextCacheManager.h"
#include "../../ui/controllers/TaskStatusController.h"
#include "../../ui/controllers/TtsController.h"
#include <QCoreApplication>
#include <QMessageBox>
#include <QInputDialog>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <algorithm>
#include "utils/Logging.h"

SequenceRunner::SequenceRunner(MainWindow* mainWindow, ISequenceService* seq, IContextService* ctx, IModelService* model,
                               IProjectService* project, IUIUpdater* ui, ITemplateService* tmpl,
                               IStatusControllerService* status, ITTSService* tts)
    : m_mainWindow(mainWindow)
    , m_seq(seq)
    , m_ctx(ctx)
    , m_model(model)
    , m_project(project)
    , m_ui(ui)
    , m_tmpl(tmpl)
    , m_status(status)
    , m_tts(tts)
{
}

void SequenceRunner::runSequenceDialog()
{
    qCDebug(logSeq) << "[runSequenceDialog] Starting sequence dialog";
    qCDebug(logSeq) << "[runSequenceDialog] Template manager root:" << m_tmpl->templateMgrPtr()->root();
    qCDebug(logSeq) << "[runSequenceDialog] About to refresh template manager";

    m_tmpl->templateMgrPtr()->refresh();
    qCDebug(logSeq) << "[runSequenceDialog] Template manager refresh completed";

    m_seq->refreshAllSequences();
    qCDebug(logSeq) << "[runSequenceDialog] refreshAllSequences completed";

    m_seq->ensureDefaultSequence();
    qCDebug(logSeq) << "[runSequenceDialog] ensureDefaultSequence completed";

    auto sequencesCopy = m_seq->seqMgrPtr()->sequences();
    int seqCount = sequencesCopy.size();
    qCDebug(logSeq) << "[runSequenceDialog] Sequence count before dialog creation:" << seqCount;

    RunTemplateOrSequenceDialog dlg(sequencesCopy, m_tmpl->templateMgrPtr(), nullptr);
    qCDebug(logSeq) << "[runSequenceDialog] Created RunTemplateOrSequenceDialog with" << seqCount << "sequences";

    if (dlg.exec() != QDialog::Accepted) {
        qCDebug(logSeq) << "[runSequenceDialog] Dialog cancelled";
        return;
    }

    qCDebug(logSeq) << "[runSequenceDialog] Dialog completed with selection";

    if (dlg.selectedMode() == RunTemplateOrSequenceDialog::RunSequence) {
        qCDebug(logSeq) << "[runSequenceDialog] Running individual sequence mode";
        const auto& currentSequences = m_seq->seqMgrPtr()->sequences();
        QString path = dlg.selectedSequencePath();
        if (!currentSequences.contains(path)) {
            QMessageBox::warning(nullptr, "Ошибка", "Файл последовательности не найден.");
            return;
        }

        m_seq->currentSeq() = currentSequences.value(path);
        m_seq->setCurrentStepIndex(0);
        m_seq->setSequenceMode(true);
        m_seq->setCurrentSeqPath(path);
        m_seq->setCurrentSeqId(m_ctx->contextManagerPtr()->startCommandSequence(m_seq->currentSeq().name(), m_seq->currentSeq().description()));
        resetSequenceStabilityState();
        m_seq->updateSequenceGraphHighlight();
        qCDebug(logSeq) << "[runSequenceDialog] About to start task timer for sequence execution";
        m_status->statusControllerPtr()->startTask();
        qCDebug(logSeq) << "[runSequenceDialog] Starting sequence:" << m_seq->currentSeq().name();
        runNextStep();
    } else {
        qCDebug(logSeq) << "[runSequenceDialog] Running template mode";
        QString templatePath = dlg.selectedTemplatePath();
        QString sequencePath = dlg.selectedDefaultSequencePath();

        if (sequencePath.isEmpty() || !QFile::exists(sequencePath)) {
            QMessageBox::warning(nullptr, "Ошибка", "Выбранная последовательность для шаблона не найдена.");
            return;
        }

        PromptSequence seq;
        QFile seqFile(sequencePath);
        if (seqFile.open(QIODevice::ReadOnly)) {
            QByteArray data = seqFile.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                seq = PromptSequence::fromJson(doc.object());
            }
        }

        if (seq.name().isEmpty()) {
            QMessageBox::warning(nullptr, "Ошибка", "Не удалось загрузить выбранную последовательность.");
            return;
        }

        m_seq->currentSeq() = seq;
        m_seq->setCurrentStepIndex(0);
        m_seq->setSequenceMode(true);
        m_seq->setCurrentSeqPath(sequencePath);
        if (m_ctx->contextManagerPtr())
            m_seq->setCurrentSeqId(m_ctx->contextManagerPtr()->startCommandSequence(m_seq->currentSeq().name(), m_seq->currentSeq().description()));
        else
            m_seq->setCurrentSeqId("");
        m_seq->setCurrentTemplatePath(templatePath);
        resetSequenceStabilityState();
        m_seq->updateSequenceGraphHighlight();
        qCDebug(logSeq) << "[runSequenceDialog] About to start task timer for template execution";
        m_status->statusControllerPtr()->startTask();
        runNextStep();
    }
}

void SequenceRunner::runNextStep()
{
    qCDebug(logSeq) << "[runNextStep] m_sequenceMode:" << m_seq->isSequenceMode() << "m_currentStepIdx:" << m_seq->currentStepIndex();
    if (!m_seq->isSequenceMode()) return;

    if (m_seq->isStopRequested()) {
        m_seq->setStopRequested(false);
        m_seq->setSequenceMode(false);
        m_seq->setCurrentSeqId("");
        m_seq->updateSequenceGraphHighlight();

        QMessageBox::information(nullptr, "Остановлено",
                                 "Выполнение шаблона остановлено пользователем.");
        return;
    }

    if (m_seq->currentStepIndex() < 0 ||
            m_seq->currentStepIndex() >= m_seq->currentSeq().steps().size()) {
        if (!m_resultShownInCurrentRun) {
            autoFinalizeSequenceWithResult(QStringLiteral("последовательность завершилась без SHOW_RESULT"), QList<Command*>());
            return;
        }
        QMessageBox::information(nullptr, "Готово",
                                 "Последовательность выполнена полностью.");
        if (!m_seq->currentSeqId().isEmpty() && m_ctx->contextManagerPtr()) {
            m_ctx->contextManagerPtr()->endCommandSequence(m_seq->currentSeqId());
        }
        m_seq->setSequenceMode(false);
        m_seq->setCurrentSeqId("");
        m_status->statusControllerPtr()->stopTask();
        m_seq->updateSequenceGraphHighlight();
        return;
    }

    const PromptStep &step = m_seq->currentSeq().steps()[m_seq->currentStepIndex()];
    PromptResolver::ResolvedPrompt resolved = PromptResolver::resolve(step.tmpl(), step, m_mainWindow);
    qCDebug(logSeq) << "[runNextStep] systemPrompt snippet:" << resolved.systemPrompt;
    qCDebug(logSeq) << "[runNextStep] userPrompt:" << resolved.userPrompt;

    {
        int tokens = m_model->modelPtr()->estimateTokens(resolved.systemPrompt + "\n" + resolved.userPrompt);
        m_ui->log(QString("[Шаг %1] Контекст ≈ %2 токенов").arg(m_seq->currentStepIndex()+1).arg(tokens));
    }
    m_ui->log(QString("\n--- Шаг %1 ------------------------------------------------")
        .arg(m_seq->currentStepIndex()+1));
    m_ui->log(resolved.systemPrompt);
    if (!resolved.userPrompt.isEmpty())
        m_ui->log(QString("USER_QUERY:\n%1").arg(resolved.userPrompt));

    if (!resolved.userPrompt.isEmpty()) {
        if (m_model->lastUserMessages().isEmpty() || m_model->lastUserMessages().last() != resolved.userPrompt) {
            m_model->history().push_back(qMakePair(QString("user"), resolved.userPrompt));
            m_model->lastUserMessages().append(resolved.userPrompt);
            while (m_model->lastUserMessages().size() > m_model->maxUserMessages()) {
                m_model->lastUserMessages().removeFirst();
            }
            qCDebug(logSeq) << "[runNextStep] Added user prompt to LAST_USER_MESSAGES storage. Current count:" << m_model->lastUserMessages().size();
        } else {
            qCDebug(logSeq) << "[runNextStep] Skipping duplicate user prompt in LAST_USER_MESSAGES storage";
        }
    }

    m_model->streamBuf().clear();

    qCDebug(logSeq) << "[DEBUG SEND TO LLM] - USER PROMPT:" << resolved.userPrompt;
    qCDebug(logSeq) << "[DEBUG SEND TO LLM] - SYSTEM PROMPT (first 500 chars):" << resolved.systemPrompt.left(500);
    qCDebug(logSeq) << "[DEBUG SEND TO LLM] - SYSTEM PROMPT LENGTH:" << resolved.systemPrompt.length();

    m_model->currentPrompt() = resolved.systemPrompt + "\n" + resolved.userPrompt;

    if (m_seq->agentRepairStepIdx() != m_seq->currentStepIndex() || m_seq->agentRepairSeqId() != m_seq->currentSeqId()) {
        m_seq->agentRepairCountThisStep() = 0;
        m_seq->setAgentRepairStepIdx(m_seq->currentStepIndex());
        m_seq->setAgentRepairSeqId(m_seq->currentSeqId());
    }

    if (m_seq->useAgentPipeline() && ensureAgentModelConfigured()) {
        QVariantMap draftOpts;
        draftOpts.insert("temperature", 0.5);
        draftOpts.insert("num_predict", 600);
        draftOpts.insert("timeoutMs", 60000);

        const QString draftSystem =
            QStringLiteral(
                "You are a senior planner.\n"
                "Return a short plan (5-10 bullets) and, if needed, 1-3 clarifying questions.\n"
                "Do NOT output JSON commands in this step.\n");

        const QString rawDraft = m_model->agentModelPtr()->generateWithSystem(resolved.userPrompt, draftSystem, draftOpts).trimmed();
        if (!rawDraft.isEmpty())
            m_ctx->appendConsoleContext(QStringLiteral("AGENT_RAW_DRAFT"), rawDraft, QString(), 0);

        QVariantMap compileOpts;
        compileOpts.insert("temperature", 0.1);
        compileOpts.insert("num_predict", 1400);
        compileOpts.insert("timeoutMs", 90000);

        QString compileSystem = resolved.systemPrompt;
        if (!rawDraft.isEmpty()) {
            compileSystem += QStringLiteral("\n\n[RAW_DRAFT]\n") + rawDraft + QStringLiteral("\n\n");
        }
        compileSystem += QStringLiteral("\n\nIMPORTANT: Output ONLY a JSON array of commands. No extra text.\n");

        const QString compiled = m_model->agentModelPtr()->generateWithSystem(resolved.userPrompt, compileSystem, compileOpts);
        QTimer::singleShot(0, nullptr, [this, compiled]() {
            m_model->streamBuf() = compiled;
            handleAssistantFinished();
        });
        return;
    }

    if (!m_model->activeImagesBase64().isEmpty()) {
        m_model->modelPtr()->startStreamWithSystemAndImages(resolved.userPrompt, resolved.systemPrompt, m_model->activeImagesBase64(), QVariantMap());
    } else {
        m_model->modelPtr()->startStreamWithSystem(resolved.userPrompt, resolved.systemPrompt);
    }
}

void SequenceRunner::handleAssistantFinished()
{
    if (m_seq->isStopRequested()) {
        m_seq->setStopRequested(false);
        m_seq->setSequenceMode(false);
        m_seq->setCurrentSeqId("");
        m_seq->updateSequenceGraphHighlight();

        QMessageBox::information(nullptr, "Остановлено",
                                 "Выполнение шаблона остановлено пользователем.");
        m_status->statusControllerPtr()->stopTask();
        qDeleteAll(CommandParser::parse(m_model->streamBuf().toUtf8()));
        return;
    }

    QByteArray fullResp = m_model->streamBuf().toUtf8();
    QByteArray jsonResp = m_model->extractJsonPart(fullResp);

    if (jsonResp.isEmpty() && m_seq->isSequenceMode() &&
        m_seq->currentStepIndex() >= 0 && m_seq->currentStepIndex() < m_seq->currentSeq().steps().size()) {
        const PromptStep &step = m_seq->currentSeq().steps()[m_seq->currentStepIndex()];
        const QByteArray repaired = repairToJsonArrayIfNeeded(fullResp, step);
        if (!repaired.isEmpty()) {
            jsonResp = repaired;
        }
    }

    const bool usedRaw = jsonResp.isEmpty();
    const QByteArray payload = usedRaw ? fullResp : jsonResp;
    if (usedRaw) {
        m_ctx->appendConsoleContext("LLM RAW OUTPUT", QString::fromUtf8(fullResp), QString(), -1);
        m_ui->log("LLM ответ сохранён как контекст: не удалось извлечь JSON. Пытаемся распарсить сырой ответ.");
    }

    m_model->history().push_back(qMakePair(QString("assistant"), QString::fromUtf8(payload)));
    m_model->lastAssistantMessage() = QString::fromUtf8(payload);

    // Check if the LLM specified to open a new sequence
    QString nextSeqName = PromptResolver::getNextSequence();
    if (!nextSeqName.isEmpty()) {
        m_ui->log("DEBUG: NEXT_SEQUENCE requested, searching for: " + nextSeqName);
        m_seq->refreshAllSequences();

        const QMap<QString, PromptSequence>& allSequences = m_seq->seqMgrPtr()->sequences();
        PromptSequence foundSeq;
        QString foundPath;

        for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
            if (it.value().name() == nextSeqName) {
                foundSeq = it.value();
                foundPath = it.key();
                m_ui->log("Found sequence by name: " + nextSeqName + " at " + foundPath);
                break;
            }
        }

        if (foundPath.isEmpty()) {
            for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
                QFileInfo fileInfo(it.key());
                if (fileInfo.fileName() == nextSeqName) {
                    foundSeq = it.value();
                    foundPath = it.key();
                    m_ui->log("Found sequence by filename: " + nextSeqName + " at " + foundPath);
                    break;
                }
            }
        }

        if (foundPath.isEmpty()) {
            QString expectedFileName = nextSeqName;
            if (!expectedFileName.endsWith(".seq.json")) {
                expectedFileName += ".seq.json";
            }
            for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
                QFileInfo fileInfo(it.key());
                if (fileInfo.fileName() == expectedFileName) {
                    foundSeq = it.value();
                    foundPath = it.key();
                    m_ui->log("Found sequence by expected filename: " + expectedFileName + " at " + foundPath);
                    break;
                }
            }
        }

        if (foundPath.isEmpty()) {
            for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
                QFileInfo fileInfo(it.key());
                if (fileInfo.baseName() == nextSeqName) {
                    foundSeq = it.value();
                    foundPath = it.key();
                    m_ui->log("Found sequence by basename match: " + nextSeqName + " at " + foundPath);
                    break;
                }
            }
        }

        if (foundPath.isEmpty()) {
            for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
                QFileInfo fileInfo(it.key());
                if (fileInfo.baseName() == nextSeqName + ".seq") {
                    foundSeq = it.value();
                    foundPath = it.key();
                    m_ui->log("Found sequence by basename + .seq match: " + nextSeqName + ".seq" + " at " + foundPath);
                    break;
                }
            }
        }

        if (foundPath.isEmpty() && !m_seq->currentSeqPath().isEmpty()) {
            QFileInfo currentSeqInfo(m_seq->currentSeqPath());
            QDir templateDir = currentSeqInfo.absoluteDir();

            QStringList possibleFilenames;
            possibleFilenames << nextSeqName;
            if (!nextSeqName.endsWith(".seq.json"))
                possibleFilenames << (nextSeqName + ".seq.json");
            if (!nextSeqName.endsWith(".json"))
                possibleFilenames << (nextSeqName + ".json");
            if (!nextSeqName.endsWith(".seq"))
                possibleFilenames << (nextSeqName + ".seq");

            for (const QString& filename : possibleFilenames) {
                QString sequenceFilePath = templateDir.filePath(filename);
                if (QFile::exists(sequenceFilePath)) {
                    QFile seqFile(sequenceFilePath);
                    if (seqFile.open(QIODevice::ReadOnly)) {
                        QByteArray data = seqFile.readAll();
                        QJsonDocument doc = QJsonDocument::fromJson(data);
                        if (doc.isObject()) {
                            foundSeq = PromptSequence::fromJson(doc.object());
                            foundPath = sequenceFilePath;
                            m_ui->log(QString("Found sequence in template directory: %1").arg(sequenceFilePath));
                            break;
                        }
                        seqFile.close();
                    }
                }
                if (!foundPath.isEmpty()) break;
            }
        }

        if (!foundPath.isEmpty()) {
            m_seq->currentSeq() = foundSeq;
            m_seq->setCurrentStepIndex(0);
            m_seq->setSequenceMode(true);
            m_seq->setCurrentSeqPath(foundPath);
            addSequenceTransitionToHistory(nextSeqName);
            m_seq->updateSequenceGraphHighlight();
            PromptResolver::clearNextSequence();
            QTimer::singleShot(200, nullptr, [this]() { runNextStep(); });
            qDeleteAll(CommandParser::parse(payload));
            return;
        }

        m_ui->log("Указанная последовательность не найдена: " + nextSeqName);
        m_ui->log("DEBUG: NEXT_SEQUENCE - Available sequences debugging:");
        m_ui->log("  Total sequences in manager: " + QString::number(allSequences.size()));
        m_ui->log("  Searched sequence name: '" + nextSeqName + "'");
        m_ui->log("  Available sequence names and paths:");
        for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
            QString seqName = it.value().name();
            QString filePath = it.key();
            QFileInfo fileInfo(filePath);
            m_ui->log("    - Sequence name: '" + seqName + "'");
            m_ui->log("    - File name: '" + fileInfo.fileName() + "'");
            m_ui->log("    - Base name: '" + fileInfo.baseName() + "'");
            m_ui->log("    - File path: " + filePath);
        }
        PromptResolver::clearNextSequence();
    }

    QVector<Command*> cmds = CommandParser::parse(payload);
    if (cmds.isEmpty()) {
        if (!usedRaw)
            m_ctx->appendConsoleContext("LLM RAW OUTPUT", QString::fromUtf8(payload), QString(), -1);
        m_ui->log("LLM не вернул команд. Сырой ответ добавлен в контекст.");

        if (m_seq->isSequenceMode()) {
            ++m_batchesWithoutResult;
            if (m_batchesWithoutResult >= 7 && !m_resultShownInCurrentRun) {
                autoFinalizeSequenceWithResult(QStringLiteral("LLM несколько шагов подряд не возвращает команды"), QList<Command*>());
                return;
            }
            if (m_seq->isStopRequested()) {
                m_seq->setStopRequested(false);
                m_seq->setSequenceMode(false);
                m_seq->setCurrentSeqId("");
                m_seq->updateSequenceGraphHighlight();

                QMessageBox::information(nullptr, "Остановлено",
                                         "Выполнение шаблона остановлено пользователем.");
                return;
            }
            m_seq->setCurrentStepIndex(m_seq->currentStepIndex() + 1);
            QTimer::singleShot(200, nullptr, [this]() { runNextStep(); });
        } else {
            retryCurrentSequenceStep();
        }
        return;
    }

    // Policy first: a command whose path leads outside the project folder, or that the
    // current step does not allow, never reaches the user or the file system. The model is
    // told why, so its next attempt can do better.
    QStringList allowedInStep;
    if (m_seq->isSequenceMode()) {
        const int stepIndex = m_seq->currentStepIndex();
        const auto& steps = m_seq->currentSeq().steps();
        if (stepIndex >= 0 && stepIndex < steps.size())
            allowedInStep = steps[stepIndex].allowedCommands();
    }
    const QString projectRoot = m_project->projectRoot();
    QList<Command*> candidates;
    QStringList rejections;
    for (Command* c : cmds) {
        QString reason;
        if (CommandPolicy::check(c, projectRoot, allowedInStep, &reason)) {
            candidates << c;
        } else {
            rejections << reason;
            delete c;
        }
    }
    if (!rejections.isEmpty()) {
        const QString feedback = QCoreApplication::translate("SequenceRunner", "Rejected commands:")
                + QStringLiteral("\n- ") + rejections.join(QStringLiteral("\n- "));
        m_ui->log(feedback);
        m_model->history().push_back(qMakePair(QString("system"), feedback));
        m_ctx->appendConsoleContext(QStringLiteral("REJECTED COMMANDS"), feedback, QString(), -1);
    }
    if (candidates.isEmpty()) {
        if (m_seq->isSequenceMode() && ++m_batchesWithoutResult >= 7 && !m_resultShownInCurrentRun) {
            autoFinalizeSequenceWithResult(QCoreApplication::translate("SequenceRunner",
                    "the model keeps proposing commands that are not allowed"), QList<Command*>());
            return;
        }
        retryCurrentSequenceStep();
        return;
    }

    // Then the user: "readonly" (the default) runs read-only batches without asking,
    // "auto" runs everything, "ask" always asks.
    const QString confirmMode = m_ui->confirmModeComboBox()->currentData().toString();
    const bool runWithoutAsking = confirmMode == QLatin1String("auto")
            || (confirmMode == QLatin1String("readonly") && CommandPolicy::isReadOnlyBatch(candidates));
    QList<Command*> toRun;
    if (runWithoutAsking) {
        toRun = candidates;
    } else {
        CommandConfirmDialog dialog(candidates, projectRoot);
        if (dialog.exec() != QDialog::Accepted) {
            m_ui->log(QCoreApplication::translate("SequenceRunner", "The commands were declined."));
            qDeleteAll(candidates);
            return;
        }
        toRun = dialog.confirmed();
        for (Command* c : candidates) {
            if (!toRun.contains(c))
                delete c;
        }
    }

    const bool hasShowResultCommand = std::any_of(toRun.begin(), toRun.end(), [](Command* c) {
        return c && c->name() == QStringLiteral("SHOW_RESULT");
    });
    const bool readOnlyBatch = isReadOnlyCommandBatch(toRun);
    const QString batchSignature = commandBatchSignature(toRun);

    bool shouldAutoFinalizeBeforeTransition = false;
    QString autoFinalizeReason;
    if (m_seq->isSequenceMode()) {
        if (hasShowResultCommand) {
            m_batchesWithoutResult = 0;
            m_sameBatchRepeatCount = 0;
            m_lastBatchSignature.clear();
        } else {
            ++m_batchesWithoutResult;
            if (!batchSignature.isEmpty() && batchSignature == m_lastBatchSignature) {
                ++m_sameBatchRepeatCount;
            } else {
                m_lastBatchSignature = batchSignature;
                m_sameBatchRepeatCount = 1;
            }

            if (readOnlyBatch && m_batchesWithoutResult >= 8) {
                shouldAutoFinalizeBeforeTransition = true;
                autoFinalizeReason = QStringLiteral("слишком много шагов без финального ответа");
            } else if (readOnlyBatch && m_sameBatchRepeatCount >= 3) {
                shouldAutoFinalizeBeforeTransition = true;
                autoFinalizeReason = QStringLiteral("повторяющиеся команды без прогресса");
            }
        }
    }

    bool hasCallSequenceCommand = false;
    bool hasOpenSequenceCommand = false;
    QString sequenceToCall;
    QString openSequenceName;

    for (Command* c : toRun) {
        if (c->name() == "ASK_USER") {
            const QString question = c->params().value("question").toString();
            bool ok = false;
            const QString ans = QInputDialog::getText(nullptr,
                                                      QStringLiteral("Ответьте на вопрос нейросети"),
                                                      question,
                                                      QLineEdit::Normal,
                                                      QString(),
                                                      &ok);
            if (ok) {
                m_model->history().push_back(qMakePair(QString("user"), ans));
                m_model->lastUserMessages().append(ans);
                while (m_model->lastUserMessages().size() > m_model->maxUserMessages()) {
                    m_model->lastUserMessages().removeFirst();
                }
            }
        } else if (c->name() == "OPEN_FILE_REGION" || c->name() == "OPEN_FILE") {
            m_ctx->handleOpenCommand(c);
        } else if (c->name() == "SHOW_RESULT") {
            QString title = c->params().value("title").toString();
            if (title.isEmpty())
                title = QStringLiteral("Ответ от %1:\n\n").arg(m_model->modelPtr()->selectedModel());
            QString content = c->params().value("content").toString() + "\n\n\n";
            if (content.isEmpty())
                content = QStringLiteral("SHOW_RESULT вызван без текста.\n\n\n");
            appendResultToUi(title, content);
            m_resultShownInCurrentRun = true;
            m_batchesWithoutResult = 0;
            m_sameBatchRepeatCount = 0;
            m_lastBatchSignature.clear();

            if (m_tts->ttsControllerPtr() && m_tts->ttsControllerPtr()->isEnabled() && m_tts->ttsControllerPtr()->runner() && m_tts->ttsControllerPtr()->runner()->isReady()) {
                QString textToSpeak = c->params().value("content").toString();
                textToSpeak.replace("\\n", " ").replace("\\r", "").replace("\n", " ");
                if (textToSpeak.length() > 1000) {
                    textToSpeak = textToSpeak.left(1000) + "...";
                }
                if (!textToSpeak.isEmpty() && textToSpeak.length() > 5) {
                    m_tts->ttsControllerPtr()->speak(textToSpeak);
                }
            }

            if (m_ctx->contextManagerPtr()) {
                m_ctx->addCommandToContext(c, c->getResult());
            }
        } else if (c->name() == "REASONING") {
            QString reasoning = c->params().value("reasoning").toString();
            if (reasoning.isEmpty())
                reasoning = c->params().value("thought_process").toString();
            if (reasoning.isEmpty())
                reasoning = c->params().value("reasoning_text").toString();

            if (!reasoning.isEmpty()) {
                if (m_ui->resultDisplayWidget()) {
                    QString formattedReasoning = reasoning;
                    auto metadata = m_seq->collectContextMetadataSince(m_ui->lastQuestionTimestamp());
                    m_ui->resultDisplayWidget()->appendResult("", formattedReasoning,
                                                    ResultDisplayWidget::EntryType::Reasoning,
                                                    metadata);
                } else {
                    m_model->history().push_back(qMakePair(QString("system"), reasoning));
                }
            }

            if (m_ctx->contextManagerPtr()) {
                m_ctx->addCommandToContext(c, c->getResult());
            }
        } else if (c->name() == "CONTEXT_SHOW") {
            m_ctx->contextCachePtr()->rebuildContextCache();
            int maxChars = c->params().value("maxChars").toInt(8000);
            if (maxChars <= 0) maxChars = 0;
            QString ctx = m_ctx->contextCachePtr()->contextContent();
            if (maxChars > 0 && ctx.size() > maxChars) {
                ctx = ctx.left(maxChars) + "\n[...truncated...]";
            }

            const QString result = QString("CONTEXT_SHOW\n[context]\n%1").arg(ctx.trimmed());

            if (m_ui->resultDisplayWidget()) {
                auto metadata = m_seq->collectContextMetadataSince(m_ui->lastQuestionTimestamp());
                m_ui->resultDisplayWidget()->appendResult(QStringLiteral("Context"), result,
                                                    ResultDisplayWidget::EntryType::Default,
                                                    metadata);
            } else {
                m_ctx->appendExternalContext(QString("==== CONTEXT_SHOW ====\n%1").arg(ctx));
            }

            m_model->history().push_back(qMakePair(QString("system"), result));
            if (m_ctx->contextManagerPtr()) {
                m_ctx->addCommandToContext(c, result);
            }
        } else if (c->name() == "CONTEXT_CLEAR") {
            const QString scope = c->params().value("scope").toString().trimmed().toLower();

            m_ctx->contextCachePtr()->clearAll();
            m_ctx->fileToContextCommands().clear();
            if (m_ctx->fileWatcher())
                m_ctx->fileWatcher()->removePaths(m_ctx->fileWatcher()->files());

            m_ctx->contextCachePtr()->clearConsoleLogs();
            m_ctx->contextCachePtr()->clearExternalSources();
            m_ctx->shellCommandOutputs().clear();
            m_ctx->pythonScriptOutputs().clear();

            if (m_ctx->contextManagerPtr()) {
                if (scope == "all")
                    m_ctx->contextManagerPtr()->clearAll();
                else
                    m_ctx->contextManagerPtr()->clearActiveContextOnly();
            }

            m_ctx->contextCachePtr()->rebuildContextCache();

            const QString result = QString("CONTEXT_CLEAR\n[scope]\n%1\n[status]\nOK").arg(scope.isEmpty() ? "active" : scope);
            m_model->history().push_back(qMakePair(QString("system"), result));
            if (m_ctx->contextManagerPtr()) {
                m_ctx->addCommandToContext(c, result);
            }
        } else if (c->name() == "CALL_SEQUENCE") {
            sequenceToCall = c->params().value("sequenceName").toString();
            hasCallSequenceCommand = true;
        } else if (c->name() == "OPEN_SEQUENCE") {
            openSequenceName = c->params().value("sequenceName").toString();
            hasOpenSequenceCommand = true;
        }
    }

    if (shouldAutoFinalizeBeforeTransition) {
        autoFinalizeSequenceWithResult(autoFinalizeReason, toRun);
        qDeleteAll(toRun);
        return;
    }

    // CALL_SEQUENCE handling
    if (hasCallSequenceCommand && !sequenceToCall.isEmpty()) {
        PromptSequence foundSeq;
        QString foundPath;
        const QMap<QString, PromptSequence>& allSequences = m_seq->seqMgrPtr()->sequences();
        for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
            if (it.value().name() == sequenceToCall) {
                foundSeq = it.value();
                foundPath = it.key();
                break;
            }
        }

        if (foundPath.isEmpty() && !m_seq->currentSeqPath().isEmpty()) {
            QFileInfo currentSeqInfo(m_seq->currentSeqPath());
            QDir templateDir = currentSeqInfo.absoluteDir();

            QStringList possibleFilenames;
            possibleFilenames << sequenceToCall;
            if (!sequenceToCall.endsWith(".seq.json"))
                possibleFilenames << (sequenceToCall + ".seq.json");
            if (!sequenceToCall.endsWith(".json"))
                possibleFilenames << (sequenceToCall + ".json");
            if (!sequenceToCall.endsWith(".seq"))
                possibleFilenames << (sequenceToCall + ".seq");

            for (const QString& filename : possibleFilenames) {
                QString sequenceFilePath = templateDir.filePath(filename);
                if (QFile::exists(sequenceFilePath)) {
                    QFile seqFile(sequenceFilePath);
                    if (seqFile.open(QIODevice::ReadOnly)) {
                        QByteArray data = seqFile.readAll();
                        QJsonDocument doc = QJsonDocument::fromJson(data);
                        if (doc.isObject()) {
                            foundSeq = PromptSequence::fromJson(doc.object());
                            foundPath = sequenceFilePath;
                            break;
                        }
                        seqFile.close();
                    }
                }
                if (!foundPath.isEmpty()) break;
            }
        }

        if (!foundPath.isEmpty()) {
            Command* callSeqCommand = nullptr;
            for (Command* c : toRun) {
                if (c->name() == "CALL_SEQUENCE") {
                    callSeqCommand = c;
                    break;
                }
            }

            bool needSummary = true;
            if (callSeqCommand && callSeqCommand->params().contains("summarize") &&
                callSeqCommand->params().value("summarize").isBool()) {
                needSummary = callSeqCommand->params().value("summarize").toBool();
            }

            if (needSummary) {
                QStringList completedStepsSummary;
                for (int i = 0; i < m_seq->currentStepIndex(); ++i) {
                    if (i < m_seq->currentSeq().steps().size()) {
                        QString stepDesc = m_seq->currentSeq().steps()[i].tmpl().section('\n', 0, 0).trimmed();
                        if (stepDesc.length() > 80) stepDesc = stepDesc.left(80) + "...";
                        completedStepsSummary << QString("Шаг %1: %2").arg(i+1).arg(stepDesc);
                    }
                }

                if (!m_seq->currentSeqId().isEmpty() && m_ctx->contextManagerPtr()) {
                    m_ctx->contextManagerPtr()->endCommandSequence(m_seq->currentSeqId());
                }

                m_seq->currentSeq() = foundSeq;
                m_seq->setCurrentStepIndex(0);
                m_seq->setSequenceMode(true);
                m_seq->setCurrentSeqId(m_ctx->contextManagerPtr()->startCommandSequence(m_seq->currentSeq().name(), m_seq->currentSeq().description()));
                m_seq->setCurrentSeqPath(foundPath);

                if (!completedStepsSummary.isEmpty()) {
                    QString summaryText = QString("Предыдущая последовательность выполнила: %1").arg(completedStepsSummary.join("; "));
                    m_model->history().push_back(qMakePair(QString("system"), summaryText));
                }

                addSequenceTransitionToHistory(sequenceToCall);
                m_seq->updateSequenceGraphHighlight();
                qDeleteAll(toRun);
                QTimer::singleShot(200, nullptr, [this]() { runNextStep(); });
                return;
            } else {
                if (!m_seq->currentSeqId().isEmpty() && m_ctx->contextManagerPtr()) {
                    m_ctx->contextManagerPtr()->endCommandSequence(m_seq->currentSeqId());
                }

                m_seq->currentSeq() = foundSeq;
                m_seq->setCurrentStepIndex(0);
                m_seq->setSequenceMode(true);
                m_seq->setCurrentSeqId(m_ctx->contextManagerPtr()->startCommandSequence(m_seq->currentSeq().name(), m_seq->currentSeq().description()));
                m_seq->setCurrentSeqPath(foundPath);

                addSequenceTransitionToHistory(sequenceToCall);
                m_seq->updateSequenceGraphHighlight();
                qDeleteAll(toRun);
                QTimer::singleShot(200, nullptr, [this]() { runNextStep(); });
                return;
            }
        }

        m_ui->log("Указанная последовательность не найдена: " + sequenceToCall);
        const QMap<QString, PromptSequence>& allSequences_debug = m_seq->seqMgrPtr()->sequences();
        m_ui->log("DEBUG: CALL_SEQUENCE - Available sequences debugging:");
        m_ui->log("  Total sequences in main manager: " + QString::number(allSequences_debug.size()));
        m_ui->log("  Current sequence path: '" + m_seq->currentSeqPath() + "'");
        if (!m_seq->currentSeqPath().isEmpty()) {
            QFileInfo currentSeqInfo(m_seq->currentSeqPath());
            QDir templateDir = currentSeqInfo.absoluteDir();
            m_ui->log("  Current sequence directory: " + templateDir.absolutePath());
            QStringList filters = {"*.seq.json", "*.seq", "*.json"};
            QFileInfoList files = templateDir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);
            m_ui->log("  Sequence files in current directory (" + QString::number(files.size()) + "):");
            for (const QFileInfo& fileInfo : files) {
                m_ui->log("    - File: " + fileInfo.fileName());
            }
        }
        m_ui->log("  Searched sequence name: '" + sequenceToCall + "'");
        m_ui->log("  Available sequence names and paths in main manager:");
        for (auto it = allSequences_debug.constBegin(); it != allSequences_debug.constEnd(); ++it) {
            QString seqName = it.value().name();
            QString filePath = it.key();
            QFileInfo fileInfo(filePath);
            m_ui->log("    - Sequence name: '" + seqName + "'");
            m_ui->log("    - File name: '" + fileInfo.fileName() + "'");
            m_ui->log("    - Base name: '" + fileInfo.baseName() + "'");
            m_ui->log("    - File path: " + filePath);
        }
    }

    // OPEN_SEQUENCE handling
    if (hasOpenSequenceCommand && !openSequenceName.isEmpty()) {
        PromptSequence foundSeq2;
        QString foundPath2;
        const QMap<QString, PromptSequence>& allSequences = m_seq->seqMgrPtr()->sequences();
        for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
            if (it.value().name() == openSequenceName) {
                foundSeq2 = it.value();
                foundPath2 = it.key();
                break;
            }
        }

        if (foundPath2.isEmpty() && !m_seq->currentSeqPath().isEmpty()) {
            QFileInfo currentSeqInfo(m_seq->currentSeqPath());
            QDir templateDir = currentSeqInfo.absoluteDir();

            QStringList possibleFilenames;
            possibleFilenames << openSequenceName;
            if (!openSequenceName.endsWith(".seq.json"))
                possibleFilenames << (openSequenceName + ".seq.json");
            if (!openSequenceName.endsWith(".json"))
                possibleFilenames << (openSequenceName + ".json");
            if (!openSequenceName.endsWith(".seq"))
                possibleFilenames << (openSequenceName + ".seq");

            for (const QString& filename : possibleFilenames) {
                QString sequenceFilePath = templateDir.filePath(filename);
                if (QFile::exists(sequenceFilePath)) {
                    QFile seqFile(sequenceFilePath);
                    if (seqFile.open(QIODevice::ReadOnly)) {
                        QByteArray data = seqFile.readAll();
                        QJsonDocument doc = QJsonDocument::fromJson(data);
                        if (doc.isObject()) {
                            foundSeq2 = PromptSequence::fromJson(doc.object());
                            foundPath2 = sequenceFilePath;
                            break;
                        }
                        seqFile.close();
                    }
                }
                if (!foundPath2.isEmpty()) break;
            }
        }

        if (!foundPath2.isEmpty()) {
            if (!m_seq->currentSeqId().isEmpty() && m_ctx->contextManagerPtr()) {
                m_ctx->contextManagerPtr()->endCommandSequence(m_seq->currentSeqId());
            }

            m_seq->currentSeq() = foundSeq2;
            m_seq->setCurrentStepIndex(0);
            m_seq->setSequenceMode(true);
            m_seq->setCurrentSeqId(m_ctx->contextManagerPtr()->startCommandSequence(m_seq->currentSeq().name(), m_seq->currentSeq().description()));
            m_seq->setCurrentSeqPath(foundPath2);

            addSequenceTransitionToHistory(openSequenceName);
            m_seq->updateSequenceGraphHighlight();
            qDeleteAll(toRun);
            QTimer::singleShot(200, nullptr, [this]() { runNextStep(); });
            return;
        }

        m_ui->log("Указанная последовательность не найдена: " + openSequenceName);
        const QMap<QString, PromptSequence>& allSequences_debug2 = m_seq->seqMgrPtr()->sequences();
        m_ui->log("DEBUG: OPEN_SEQUENCE - Available sequences debugging:");
        m_ui->log("  Total sequences in main manager: " + QString::number(allSequences_debug2.size()));
        m_ui->log("  Current sequence path: '" + m_seq->currentSeqPath() + "'");
        if (!m_seq->currentSeqPath().isEmpty()) {
            QFileInfo currentSeqInfo(m_seq->currentSeqPath());
            QDir templateDir = currentSeqInfo.absoluteDir();
            m_ui->log("  Current sequence directory: " + templateDir.absolutePath());
            QStringList filters = {"*.seq.json", "*.seq", "*.json"};
            QFileInfoList files = templateDir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);
            m_ui->log("  Sequence files in current directory (" + QString::number(files.size()) + "):");
            for (const QFileInfo& fileInfo : files) {
                m_ui->log("    - File: " + fileInfo.fileName());
            }
        }
        m_ui->log("  Searched sequence name: '" + openSequenceName + "'");
        m_ui->log("  Available sequence names and paths in main manager:");
        for (auto it = allSequences_debug2.constBegin(); it != allSequences_debug2.constEnd(); ++it) {
            QString seqName = it.value().name();
            QString filePath = it.key();
            QFileInfo fileInfo(filePath);
            m_ui->log("    - Sequence name: '" + seqName + "'");
            m_ui->log("    - File name: '" + fileInfo.fileName() + "'");
            m_ui->log("    - Base name: '" + fileInfo.baseName() + "'");
            m_ui->log("    - File path: " + filePath);
        }
    }

    // Apply commands
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
        m_seq->prevSteps() << QString("Шаг %1: %2").arg(m_seq->currentStepIndex() + 1).arg(appliedSummary.join("; "));
    }

    QString err;
    auto logConsoleResult = [&](Command* cmd, const QString& errorMessage = QString()){
        QString result = cmd->getResult();

        if (!result.isEmpty()) {
            QString fullTitle = result.split('\n').first();
            m_ctx->appendConsoleContext(fullTitle, result, QString(), errorMessage.isEmpty() ? (cmd->wasSuccessful() ? 0 : 1) : 1);

            m_model->history().push_back(qMakePair(QString("system"), result));
        } else if (!errorMessage.isEmpty()) {
            // Tell the model why its command failed, so the next step can correct it.
            m_model->history().push_back(qMakePair(QString("system"),
                    QStringLiteral("%1 failed: %2").arg(cmd->name(), errorMessage)));
        }

        if (m_ctx->contextManagerPtr()) {
            m_ctx->addCommandToContext(cmd, result);
        }
    };

    QList<Command*> pendingEdits;
    bool applyFailed = false;
    const QDateTime execStartedAt = QDateTime::currentDateTime();
    OllamaAdapter* execModel = m_model->modelPtr();
    if (m_seq->useAgentPipeline() && ensureAgentModelConfigured())
        execModel = m_model->agentModelPtr();
    for (Command* c : toRun) {
        const QString name = c->name();
        if (name == "ASK_USER" || name == "OPEN_FILE" || name == "OPEN_FILE_REGION" || name == "SHOW_RESULT" || name == "REASONING"
            || name == "CONTEXT_SHOW" || name == "CONTEXT_CLEAR")
            continue;
        if (name == "EDIT_IN_RANGE") {
            pendingEdits << c;
            continue;
        }
        if (!c->applyWithLLM(m_project->devManagerPtr(), execModel, &err)) {
            c->setExecutionResult(false, err);
            logConsoleResult(c, err);
            QMessageBox::warning(nullptr, QStringLiteral("Ошибка выполнения"), err);
            applyFailed = true;
            break;
        }
        c->setExecutionResult(true);
        m_ctx->handlePostApplyContextCleanup(c);
        logConsoleResult(c, QString());
        m_ctx->handleSearchResults(c);
    }

    if (!applyFailed && !pendingEdits.isEmpty()) {
        if (!m_project->applyGroupedEditInRange(pendingEdits, &err)) {
            QMessageBox::warning(nullptr, QStringLiteral("Ошибка выполнения"), err);
        } else {
            for (Command* c : pendingEdits) {
                m_ctx->handlePostApplyContextCleanup(c);
                m_ctx->handleSearchResults(c);
            }
        }
    }

    m_project->rescan();

    if (applyFailed && !m_seq->isSequenceMode()) {
        retryCurrentSequenceStep();
    } else if (m_seq->isSequenceMode()) {
        if (m_seq->isStopRequested()) {
            m_seq->setStopRequested(false);
            m_seq->setSequenceMode(false);
            m_seq->setCurrentSeqId("");
            m_seq->updateSequenceGraphHighlight();

            QMessageBox::information(nullptr, "Остановлено",
                                     "Выполнение шаблона остановлено пользователем.");
            qDeleteAll(toRun);
            return;
        }

        const PromptStep &step = m_seq->currentSeq().steps()[m_seq->currentStepIndex()];
        if (maybeAutoRepairAfterExecution(step, execStartedAt, toRun, applyFailed, err)) {
            qDeleteAll(toRun);
            return;
        }
        const bool finishingCurrentStep = !step.loop() && (m_seq->currentStepIndex() + 1 >= m_seq->currentSeq().steps().size());
        if (finishingCurrentStep && !hasShowResultCommand && !m_resultShownInCurrentRun) {
            autoFinalizeSequenceWithResult(QStringLiteral("финальный шаг не вернул SHOW_RESULT"), toRun);
            qDeleteAll(toRun);
            return;
        }
        if (step.loop()) {
            QTimer::singleShot(200, nullptr, [this]() { runNextStep(); });
        } else {
            m_seq->setCurrentStepIndex(m_seq->currentStepIndex() + 1);
            QTimer::singleShot(200, nullptr, [this]() { runNextStep(); });
        }
    }

    qDeleteAll(toRun);
}

void SequenceRunner::resetSequenceStabilityState()
{
    m_resultShownInCurrentRun = false;
    m_batchesWithoutResult = 0;
    m_sameBatchRepeatCount = 0;
    m_lastBatchSignature.clear();
}

QString SequenceRunner::commandBatchSignature(const QList<Command*>& commands) const
{
    QStringList parts;
    for (Command* cmd : commands) {
        if (!cmd)
            continue;
        const QJsonObject p = cmd->params();
        QString marker;
        if (p.contains("query"))
            marker = p.value("query").toString().trimmed();
        else if (p.contains("sequenceName"))
            marker = p.value("sequenceName").toString().trimmed();
        else if (p.contains("path"))
            marker = p.value("path").toString().trimmed();
        else if (p.contains("command"))
            marker = p.value("command").toString().trimmed();
        else if (p.contains("script"))
            marker = p.value("script").toString().trimmed();

        if (marker.size() > 120)
            marker = marker.left(120);
        parts << QString("%1:%2").arg(cmd->name(), marker);
    }
    return parts.join("|");
}

bool SequenceRunner::isReadOnlyCommandBatch(const QList<Command*>& commands) const
{
    if (commands.isEmpty())
        return false;

    static const QSet<QString> readOnlyCommands = {
        QStringLiteral("SEARCH_WEB"),
        QStringLiteral("SEARCH_PROJECT"),
        QStringLiteral("SEARCH_PROJECT_CASE_INSENSITIVE"),
        QStringLiteral("OPEN_FILE"),
        QStringLiteral("OPEN_FILE_REGION"),
        QStringLiteral("OPEN_SEQUENCE"),
        QStringLiteral("CALL_SEQUENCE"),
        QStringLiteral("LIST_FILES_IN_FOLDER"),
        QStringLiteral("CONTEXT_SHOW"),
        QStringLiteral("REASONING"),
        QStringLiteral("WRITE_DESCRIPTION"),
        QStringLiteral("CLIPBOARD_GET")
    };

    for (Command* cmd : commands) {
        if (!cmd)
            continue;
        if (!readOnlyCommands.contains(cmd->name()))
            return false;
    }
    return true;
}

void SequenceRunner::appendResultToUi(const QString& title, const QString& content)
{
    if (m_ui->resultDisplayWidget()) {
        auto metadata = m_seq->collectContextMetadataSince(m_ui->lastQuestionTimestamp());
        m_ui->resultDisplayWidget()->appendResult(title, content,
                                            ResultDisplayWidget::EntryType::Default,
                                            metadata);
    } else {
        m_ctx->appendExternalContext(QString("==== SHOW_RESULT ====\n%1").arg(content));
    }
}

void SequenceRunner::autoFinalizeSequenceWithResult(const QString& reason, const QList<Command*>& recentCommands)
{
    QStringList lines;
    Q_UNUSED(reason);
    lines << QStringLiteral("Я завершил выполнение автоматически, чтобы не допустить зацикливания.");
    if (!m_model->lastUserMessage().trimmed().isEmpty())
        lines << QStringLiteral("Запрос: %1").arg(m_model->lastUserMessage().trimmed());

    QStringList sourceLines;
    QStringList commandNames;
    for (Command* cmd : recentCommands) {
        if (!cmd)
            continue;
        commandNames << cmd->name();

        if (auto* web = dynamic_cast<SearchWebCommand*>(cmd)) {
            for (const QString& block : web->results()) {
                if (sourceLines.size() >= 5)
                    break;
                const QStringList parts = block.split('\n', QString::SkipEmptyParts);
                if (parts.size() < 2)
                    continue;
                const QString title = parts.at(0).trimmed();
                const QString link = parts.at(1).trimmed();
                if (title.isEmpty() || link.isEmpty() || !link.startsWith(QStringLiteral("http")))
                    continue;
                sourceLines << QString("%1 - %2").arg(title, link);
            }
        }
    }
    if (!sourceLines.isEmpty()) {
        lines << QStringLiteral("Найденные источники:");
        for (const QString& s : sourceLines)
            lines << QString("- %1").arg(s);
    } else if (!commandNames.isEmpty()) {
        lines << QStringLiteral("Последние выполненные команды: %1").arg(commandNames.join(", "));
    }

    lines << QStringLiteral("Если нужно, сформулируйте запрос чуть точнее: период, регион, источники.");
    const QString content = lines.join(QStringLiteral("\n\n")) + QStringLiteral("\n\n\n");
    appendResultToUi(QStringLiteral("Ответ нейросети"), content);
    m_resultShownInCurrentRun = true;

    if (!m_seq->currentSeqId().isEmpty() && m_ctx->contextManagerPtr())
        m_ctx->contextManagerPtr()->endCommandSequence(m_seq->currentSeqId());
    m_seq->setCurrentSeqId("");
    m_seq->setSequenceMode(false);
    m_seq->updateSequenceGraphHighlight();
    m_status->statusControllerPtr()->stopTask();
}

void SequenceRunner::retryCurrentSequenceStep()
{
    if (!m_seq->isSequenceMode())
        return;

    if (m_seq->isStopRequested()) {
        m_seq->setStopRequested(false);
        m_seq->setSequenceMode(false);
        m_seq->setCurrentSeqId("");
        m_seq->updateSequenceGraphHighlight();

        QMessageBox::information(nullptr, "Остановлено",
                                 "Выполнение шаблона остановлено пользователем.");
        return;
    }

    QTimer::singleShot(200, nullptr, [this]() { runNextStep(); });
}

bool SequenceRunner::ensureAgentModelConfigured()
{
    if (!m_seq->useAgentPipeline())
        return false;

    // The agent uses the model chosen in the chat unless one was set for it; there is no
    // hidden default model that the user may not have installed.
    if (m_model->agentModelPtr()->selectedModel().isEmpty()) {
        const QString chatModel = m_model->modelPtr()->selectedModel();
        if (chatModel.isEmpty())
            return false;
        m_model->agentModelPtr()->configure(chatModel, QVariantMap());
    }

    QString err;
    if (!m_model->agentModelPtr()->isAvailable(&err)) {
        m_ui->log(QString("Agent model unavailable (%1). Falling back to selected model.").arg(err));
        return false;
    }
    return true;
}

QStringList SequenceRunner::buildAllowedCommandsDocsCompact(const PromptStep& step) const
{
    QStringList out;
    const QSet<QString> allowed = QSet<QString>::fromList(step.allowedCommands());
    for (Command* c : CommandRegistry::all()) {
        const QString name = c->name();
        if (!allowed.contains(name)) {
            delete c;
            continue;
        }
        const CommandDoc doc = CommandDocs::doc(name);
        out << CommandDocs::renderForLLM(doc, true);
        delete c;
    }
    return out;
}

QByteArray SequenceRunner::repairToJsonArrayIfNeeded(const QByteArray& rawText, const PromptStep& step)
{
    const QByteArray extracted = m_model->extractJsonPart(rawText);
    if (!extracted.isEmpty())
        return extracted;

    if (!ensureAgentModelConfigured())
        return QByteArray();

    const QStringList allowedDocs = buildAllowedCommandsDocsCompact(step);
    const QString systemPrompt =
        QStringLiteral(
            "You are a strict JSON repair tool.\n"
            "Task: Convert the INPUT into a valid JSON array of commands.\n"
            "Rules:\n"
            "- Output ONLY a JSON array. No markdown, no comments.\n"
            "- Each element is an object with keys: command, params.\n"
            "- params MUST be a JSON object (not a string).\n"
            "- Only use commands from the allowed list below.\n"
            "- If there is not enough info to act, output exactly one ASK_USER command.\n"
            "- If no actions are needed, output [].\n\n"
            "Allowed commands:\n%1\n")
            .arg(allowedDocs.join(QStringLiteral("\n")));

    QVariantMap opts;
    opts.insert("temperature", 0.05);
    opts.insert("num_predict", 800);
    opts.insert("timeoutMs", 45000);

    const QString repaired = m_model->agentModelPtr()->generateWithSystem(QString::fromUtf8(rawText), systemPrompt, opts);
    const QByteArray repairedUtf8 = repaired.toUtf8();
    const QByteArray repairedJson = m_model->extractJsonPart(repairedUtf8);
    return repairedJson.isEmpty() ? QByteArray() : repairedJson;
}

bool SequenceRunner::isImplementationIntentText(const QString& text)
{
    const QString t = text.toLower();
    static const QStringList needles = {
        "реализ", "сделай", "создай", "добав", "исправ", "почин", "внедр", "напис",
        "сгенер", "передел", "улучш", "доработ", "перепиш",
        "implement", "create", "add", "fix", "repair", "refactor", "rewrite", "build", "generate"
    };
    for (const QString& n : needles) {
        if (t.contains(n))
            return true;
    }
    return false;
}

static QString normalizePathForProject(const QString& raw, const QString& projectRoot)
{
    if (raw.trimmed().isEmpty())
        return QString();
    QFileInfo info(raw);
    if (info.isAbsolute())
        return QDir::cleanPath(info.absoluteFilePath());
    return QDir::cleanPath(QDir(projectRoot).absoluteFilePath(raw));
}

bool SequenceRunner::didAnyAffectedPathChangeSince(const QDateTime& since, const QList<Command*>& executedCommands) const
{
    if (!since.isValid())
        return false;

    static const QStringList keys = {
        "path", "oldPath", "newPath", "folderPath", "scriptPath", "sourceFilePath"
    };

    QSet<QString> touched;
    for (const Command* c : executedCommands) {
        if (!c)
            continue;
        const QJsonObject params = c->params();
        for (const QString& k : keys) {
            const QString raw = params.value(k).toString();
            const QString p = normalizePathForProject(raw, m_project->projectRoot());
            if (!p.isEmpty())
                touched.insert(p);
        }
    }

    for (const QString& p : touched) {
        QFileInfo fi(p);
        if (fi.exists() && fi.lastModified() >= since)
            return true;
    }
    return false;
}

bool SequenceRunner::maybeAutoRepairAfterExecution(const PromptStep& step,
                                                  const QDateTime& execStartedAt,
                                                  const QList<Command*>& executedCommands,
                                                  bool applyFailed,
                                                  const QString& lastError)
{
    if (!m_seq->isSequenceMode())
        return false;
    if (!m_seq->useAgentPipeline())
        return false;
    if (m_seq->inAgentAutoRepair())
        return false;

    if (m_seq->agentRepairStepIdx() != m_seq->currentStepIndex() || m_seq->agentRepairSeqId() != m_seq->currentSeqId()) {
        m_seq->agentRepairCountThisStep() = 0;
        m_seq->setAgentRepairStepIdx(m_seq->currentStepIndex());
        m_seq->setAgentRepairSeqId(m_seq->currentSeqId());
    }

    if (m_seq->agentRepairCountThisStep() >= m_seq->agentMaxRepairs())
        return false;

    const bool implIntent = isImplementationIntentText(m_model->lastUserMessage());
    const bool changed = didAnyAffectedPathChangeSince(execStartedAt, executedCommands);
    const bool needRepair = applyFailed || (implIntent && !changed);
    if (!needRepair)
        return false;

    if (!ensureAgentModelConfigured())
        return false;

    const QStringList allowedDocs = buildAllowedCommandsDocsCompact(step);
    const QString ctx = m_ctx->contextManagerPtr() ? m_ctx->contextManagerPtr()->generateContextText() : QString();
    const QString systemPrompt =
        QStringLiteral(
            "You are an expert developer agent that must output ONLY a JSON array of commands.\n"
            "Goal: make real progress towards the USER REQUEST using the allowed commands.\n"
            "Rules:\n"
            "- Output ONLY JSON array, no extra text.\n"
            "- Use as few commands as possible (1-5).\n"
            "- Prefer file-editing commands when implementation is requested.\n"
            "- If missing critical info, output exactly one ASK_USER.\n"
            "- Do NOT loop on CALL_SEQUENCE.\n\n"
            "Allowed commands:\n%1\n")
            .arg(allowedDocs.join(QStringLiteral("\n")));

    QString userPrompt = QStringLiteral("USER REQUEST:\n%1\n").arg(m_model->lastUserMessage());
    if (applyFailed && !lastError.trimmed().isEmpty()) {
        userPrompt += QStringLiteral("\nLAST ERROR:\n%1\n").arg(lastError);
    }
    if (!ctx.trimmed().isEmpty()) {
        userPrompt += QStringLiteral("\nRECENT CONTEXT (executed commands/results):\n%1\n").arg(ctx.left(12000));
    }

    QVariantMap opts;
    opts.insert("temperature", 0.1);
    opts.insert("num_predict", 1200);
    opts.insert("timeoutMs", 60000);

    const QString repair = m_model->agentModelPtr()->generateWithSystem(userPrompt, systemPrompt, opts);
    const QByteArray repairedJson = m_model->extractJsonPart(repair.toUtf8());
    if (repairedJson.isEmpty())
        return false;

    ++m_seq->agentRepairCountThisStep();
    m_seq->setInAgentAutoRepair(true);
    m_ctx->appendConsoleContext(QStringLiteral("AGENT_REPAIR_DRAFT"), repair, QString(), 0);

    QTimer::singleShot(0, nullptr, [this, repairedJson]() {
        m_model->streamBuf() = QString::fromUtf8(repairedJson);
        handleAssistantFinished();
        m_seq->setInAgentAutoRepair(false);
    });
    return true;
}

void SequenceRunner::runNextSequence()
{
    if (!m_seq->isSequenceMode()) return;
    if (m_seq->currentStepIndex() < 0 ||
            m_seq->currentStepIndex() >= m_seq->currentSeq().steps().size()) {
        QMessageBox::information(nullptr, "Готово",
                                 "Последовательность выполнена полностью.");
        if (!m_seq->currentSeqId().isEmpty() && m_ctx->contextManagerPtr()) {
            m_ctx->contextManagerPtr()->endCommandSequence(m_seq->currentSeqId());
        }
        m_seq->setSequenceMode(false);
        m_seq->setCurrentSeqId("");
        m_status->statusControllerPtr()->stopTask();
        m_seq->updateSequenceGraphHighlight();
        return;
    }

    const PromptStep &step = m_seq->currentSeq().steps()[m_seq->currentStepIndex()];
    PromptResolver::ResolvedPrompt resolved = PromptResolver::resolve(step.tmpl(), step, m_mainWindow);
    qCDebug(logSeq) << "[runNextSequence] systemPrompt snippet:" << resolved.systemPrompt.left(200);
    qCDebug(logSeq) << "[runNextSequence] userPrompt:" << resolved.userPrompt;

    {
        int tokens = m_model->modelPtr()->estimateTokens(resolved.systemPrompt + "\n" + resolved.userPrompt);
        m_ui->log(QString("[Шаг %1] Контекст ≈ %2 токенов").arg(m_seq->currentStepIndex()+1).arg(tokens));
    }
    m_ui->log(QString("\n--- Шаг %1 ------------------------------------------------")
        .arg(m_seq->currentStepIndex()+1));
    m_ui->log(resolved.systemPrompt);
    if (!resolved.userPrompt.isEmpty())
        m_ui->log(QString("USER_QUERY:\n%1").arg(resolved.userPrompt));

    m_model->streamBuf().clear();

    qCDebug(logSeq) << "[DEBUG SEND TO LLM] - USER PROMPT:" << resolved.userPrompt;
    qCDebug(logSeq) << "[DEBUG SEND TO LLM] - SYSTEM PROMPT (first 500 chars):" << resolved.systemPrompt.left(500);
    qCDebug(logSeq) << "[DEBUG SEND TO LLM] - SYSTEM PROMPT LENGTH:" << resolved.systemPrompt.length();

    m_model->currentPrompt() = resolved.systemPrompt + "\n" + resolved.userPrompt;

    if (m_seq->useAgentPipeline() && ensureAgentModelConfigured()) {
        QVariantMap compileOpts;
        compileOpts.insert("temperature", 0.1);
        compileOpts.insert("num_predict", 1400);
        compileOpts.insert("timeoutMs", 90000);

        const QString compileSystem = resolved.systemPrompt + QStringLiteral("\n\nIMPORTANT: Output ONLY a JSON array of commands. No extra text.\n");
        const QString compiled = m_model->agentModelPtr()->generateWithSystem(resolved.userPrompt, compileSystem, compileOpts);
        QTimer::singleShot(0, nullptr, [this, compiled]() {
            m_model->streamBuf() = compiled;
            handleAssistantFinished();
        });
        return;
    }

    if (!m_model->activeImagesBase64().isEmpty()) {
        m_model->modelPtr()->startStreamWithSystemAndImages(resolved.userPrompt, resolved.systemPrompt, m_model->activeImagesBase64(), QVariantMap());
    } else {
        m_model->modelPtr()->startStreamWithSystem(resolved.userPrompt, resolved.systemPrompt);
    }
}

void SequenceRunner::addSequenceTransitionToHistory(const QString& sequenceName)
{
    QString transitionMsg = QString("Переход к последовательности: %1").arg(sequenceName);
    m_model->history().push_back(qMakePair(QString("system"), transitionMsg));
}

void SequenceRunner::appendUserQuestionToResultDisplay(const QString& question)
{
    if (!m_ui->resultDisplayWidget())
        return;

    const QString trimmedQuestion = question.trimmed();
    if (trimmedQuestion.isEmpty())
        return;

    m_ui->lastQuestionTimestamp() = QDateTime::currentDateTime();

    const QString formattedQuestion = QString("\n\n%1\n").arg(trimmedQuestion);
    m_ui->resultDisplayWidget()->appendResult(QStringLiteral("Вопрос пользователя"),
                                        formattedQuestion,
                                        ResultDisplayWidget::EntryType::UserQuestion);
}

void SequenceRunner::stopExecution()
{
    m_seq->setStopRequested(true);

    if (m_seq->isSequenceMode()) {
        m_ui->log("Шаблонное выполнение остановлено пользователем.");
        m_status->statusControllerPtr()->stopTask();
    }
}

void SequenceRunner::handleOpenCommand(Command* cmd)
{
    const QJsonObject params = cmd->params();
    const QString rawPath = params.value("path").toString();
    if (rawPath.isEmpty())
        return;

    const QString normalized = normalizeContextPath(rawPath, m_project->projectRoot());
    QFile file(normalized);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    const QString text = QString::fromUtf8(file.readAll());
    file.close();

    QStringList allLines = text.split('\n', QString::KeepEmptyParts);
    if (text.endsWith('\n') && !allLines.isEmpty())
        allLines.removeLast();
    if (cmd->name() == "OPEN_FILE") {
        m_ctx->contextCachePtr()->setFullFileContext(normalized, allLines);
        m_ctx->fileToContextCommands()[normalized].insert("OPEN_FILE");
        if (!m_ctx->fileWatcher()->files().contains(normalized) && QFileInfo::exists(normalized)) {
            m_ctx->fileWatcher()->addPath(normalized);
        }
        if (m_ctx->contextManagerPtr()) {
            QString result = QString("OPEN_FILE: %1 (full content)").arg(rawPath);
            m_ctx->addCommandToContext(cmd, result);
        }
        return;
    }

    int startLine = params.value("startLine").toInt(-1);
    int endLine = params.value("endLine").toInt(-1);
    QStringList snippet;

    if (startLine >= 1 && endLine >= startLine) {
        if (startLine > allLines.size()) {
            QString errorMsg = QString("ERROR: Cannot OPEN_FILE_REGION %1: requested start line %2 is beyond file length of %3 lines. Remember: line numbering starts from 1, not 0.")
                    .arg(rawPath).arg(startLine).arg(allLines.size());
            m_ui->log(errorMsg);
            if (m_ctx->contextManagerPtr()) {
                m_ctx->addCommandToContext(cmd, errorMsg);
            }
            return;
        }

        if (endLine > allLines.size()) {
            QString errorMsg = QString("ERROR: Cannot OPEN_FILE_REGION %1: requested end line %2 is beyond file length of %3 lines. Remember: line numbering starts from 1, not 0.")
                    .arg(rawPath).arg(endLine).arg(allLines.size());
            m_ui->log(errorMsg);
            if (m_ctx->contextManagerPtr()) {
                m_ctx->addCommandToContext(cmd, errorMsg);
            }
            return;
        }

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

    if (!snippet.isEmpty()) {
        m_ctx->contextCachePtr()->addRegionContext(normalized, startLine, snippet);
        m_ctx->fileToContextCommands()[normalized].insert("OPEN_FILE_REGION");
        if (!m_ctx->fileWatcher()->files().contains(normalized) && QFileInfo::exists(normalized)) {
            m_ctx->fileWatcher()->addPath(normalized);
        }
        if (m_ctx->contextManagerPtr()) {
            QString numberedContent = m_ctx->contextCachePtr()->formatNumberedLines(snippet, startLine);
            QString result = QString("OPEN_FILE_REGION: %1\n%2").arg(rawPath, numberedContent);
            m_ctx->addCommandToContext(cmd, result);
        }
    }
}

bool SequenceRunner::applyGroupedEditInRange(const QList<Command*>& commands, QString* err)
{
    QMap<QString, QList<EditInRangeCommand*>> grouped;
    for (Command* c : commands) {
        auto edit = dynamic_cast<EditInRangeCommand*>(c);
        if (!edit)
            continue;
        const QString normalized = normalizeContextPath(edit->path(), m_project->projectRoot());
        grouped[normalized].append(edit);
    }

    for (auto it = grouped.begin(); it != grouped.end(); ++it) {
        if (!EditInRangeCommand::applyBatch(it.value(), m_project->devManagerPtr(), err))
            return false;
        m_ctx->contextCachePtr()->clearContextForPath(it.key());
    }
    return true;
}

void SequenceRunner::handlePostApplyContextCleanup(Command* cmd)
{
    const QString name = cmd->name();
    const QJsonObject p = cmd->params();
    auto clear = [&](const QString& key) {
        if (!key.isEmpty())
            m_ctx->contextCachePtr()->clearContextForPath(key);
    };

    if (name == "EDIT_IN_RANGE" || name == "REPLACE_IN_FILE"
        || name == "DELETE_FILE" || name == "CREATE_FILE"
        || name == "APPEND_TO_FILE" || name == "INSERT_AT_LINE") {
        clear(p.value("path").toString());
        return;
    }

    if (name == "RENAME_FILE") {
        clear(p.value("oldPath").toString());
        clear(p.value("newPath").toString());
        return;
    }

    if (name == "COPY_FILE") {
        clear(p.value("to").toString());
        return;
    }

    if (name == "GIT_RESTORE_FILE") {
        clear(p.value("path").toString());
        return;
    }

    if (name == "APPLY_PATCH") {
        m_ctx->contextCachePtr()->clearAll();
        return;
    }

    if (name == "UNDO_LAST_CHANGE") {
        m_ctx->contextCachePtr()->clearAll();
        return;
    }

    if (name == "RUN_SHELL") {
        QString command = p.value("command").toString();
        if (!command.isEmpty()) {
            m_ctx->contextCachePtr()->removeConsoleLogsWithPrefix("==== RUN_SHELL: " + command);
            m_ctx->shellCommandOutputs().remove(command);
        }
        return;
    }

    if (name == "RUN_PYTHON") {
        QString scriptPath = p.value("scriptPath").toString();
        QString code = p.value("code").toString();
        QString identifier = !scriptPath.isEmpty() ? scriptPath : "inline code";

        if (!identifier.isEmpty()) {
            m_ctx->contextCachePtr()->removeConsoleLogsWithPrefix("==== RUN_PYTHON: " + identifier);
            m_ctx->pythonScriptOutputs().remove(identifier);
        }
        return;
    }

    if (name == "OPEN_FILE" || name == "OPEN_FILE_REGION") {
        QString path = p.value("path").toString();
        if (!path.isEmpty()) {
            const QString normalized = normalizeContextPath(path, m_project->projectRoot());
            if (!m_ctx->fileWatcher()->files().contains(normalized) && QFileInfo::exists(normalized)) {
                m_ctx->fileWatcher()->addPath(normalized);
            }
        }
    }
}

void SequenceRunner::handleSearchResults(Command* cmd)
{
    if (auto web = dynamic_cast<SearchWebCommand*>(cmd)) {
        if (web->results().isEmpty())
            return;
        QString query = cmd->params().value("query").toString();
        QString title = QString("==== SEARCH_WEB: %1 ====").arg(query);
        QString body = web->results().join("\n\n");
        qCDebug(logSeq) << "[MainWindow] SEARCH_WEB results count:" << web->results().size();
        qCDebug(logSeq) << "[MainWindow] SEARCH_WEB result body length:" << body.length();
        if (!body.isEmpty()) {
            qCDebug(logSeq) << "[MainWindow] SEARCH_WEB first 200 chars of result:" << body.left(200);
        } else {
            qCDebug(logSeq) << "[MainWindow] SEARCH_WEB result body is empty";
        }

        bool needsSummarization = body.contains("Контент для суммаризации", Qt::CaseInsensitive);

        QString resultBody = body;
        if (needsSummarization) {
            resultBody = m_model->summarizeSearchResults(body, query);
        }

        appendExternalContext(QString("%1\n%2").arg(title, resultBody));

        if (m_ctx->contextManagerPtr()) {
            qCDebug(logSeq) << "[MainWindow] Adding SEARCH_WEB command to context manager";
            m_ctx->addCommandToContext(cmd, QString("%1\n%2").arg(title, resultBody));
        } else {
            qCDebug(logSeq) << "[MainWindow] Context manager is null, skipping context addition";
        }
        return;
    }
    if (auto local = dynamic_cast<SearchProjectCommand*>(cmd)) {
        if (local->matches().isEmpty())
            return;
        QString title = QString("==== SEARCH_PROJECT: %1 ====").arg(cmd->params().value("query").toString());
        QString body = local->matches().join("\n\n");
        appendExternalContext(QString("%1\n%2").arg(title, body));

        if (m_ctx->contextManagerPtr()) {
            m_ctx->addCommandToContext(cmd, QString("%1\n%2").arg(title, body));
        }
    }
}

void SequenceRunner::appendConsoleContext(const QString& title,
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

    if (title.startsWith("RUN_SHELL: ")) {
        QString cmd = title.mid(11);
        m_ctx->contextCachePtr()->removeConsoleLogsWithPrefix("==== RUN_SHELL: " + cmd);
        m_ctx->shellCommandOutputs().remove(cmd);
    }
    else if (title.startsWith("RUN_PYTHON: ")) {
        QString script = title.mid(12);
        m_ctx->contextCachePtr()->removeConsoleLogsWithPrefix("==== RUN_PYTHON: " + script);
        m_ctx->pythonScriptOutputs().remove(script);
    }

    m_ctx->contextCachePtr()->appendConsoleLog(block.trimmed());
    while (m_ctx->contextCachePtr()->consoleLogs().size() > 5) {
        auto logs = m_ctx->contextCachePtr()->consoleLogs();
        m_ctx->contextCachePtr()->clearConsoleLogs();
        for (int i = 0; i < qMin(5, logs.size()); ++i) {
            m_ctx->contextCachePtr()->appendConsoleLog(logs.at(i));
        }
    }
}

void SequenceRunner::appendExternalContext(const QString& block)
{
    if (block.trimmed().isEmpty())
        return;
    m_ctx->contextCachePtr()->appendExternalSource(block.trimmed());
    while (m_ctx->contextCachePtr()->externalSources().size() > 5) {
        auto sources = m_ctx->contextCachePtr()->externalSources();
        m_ctx->contextCachePtr()->clearExternalSources();
        for (int i = 0; i < qMin(5, sources.size()); ++i) {
            m_ctx->contextCachePtr()->appendExternalSource(sources.at(i));
        }
    }
}
