#include "SequenceRunner.h"
#include "../MainWindow.h"
#include "../../adapters/OllamaAdapter.h"
#include "../../command/CommandParser.h"
#include "../../command/CommandRegistry.h"
#include "../../command/CommandDocs.h"
#include "../../command/EditInRangeCommand.h"
#include "../../command/SearchWebCommand.h"
#include "../../core/PromptSequenceManager.h"
#include "../../core/PromptSequence.h"
#include "../../core/PromptResolver.h"
#include "../../core/DevAssManager.h"
#include "../../ui/ResultDisplayWidget.h"
#include "../../ui/customWidgets/MemoryComboBox.h"
#include "../../ui/RunTemplateOrSequenceDialog.h"
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

SequenceRunner::SequenceRunner(MainWindow* mw)
    : m_mw(mw)
{
}

void SequenceRunner::runSequenceDialog()
{
    qDebug() << "[runSequenceDialog] Starting sequence dialog";
    qDebug() << "[runSequenceDialog] Template manager root:" << m_mw->m_templateMgr.root();
    qDebug() << "[runSequenceDialog] About to refresh template manager";

    m_mw->m_templateMgr.refresh();
    qDebug() << "[runSequenceDialog] Template manager refresh completed";

    m_mw->refreshAllSequences();
    qDebug() << "[runSequenceDialog] refreshAllSequences completed";

    m_mw->ensureDefaultSequence();
    qDebug() << "[runSequenceDialog] ensureDefaultSequence completed";

    auto sequencesCopy = m_mw->m_seqMgr.sequences();
    int seqCount = sequencesCopy.size();
    qDebug() << "[runSequenceDialog] Sequence count before dialog creation:" << seqCount;

    RunTemplateOrSequenceDialog dlg(sequencesCopy, &m_mw->m_templateMgr, m_mw);
    qDebug() << "[runSequenceDialog] Created RunTemplateOrSequenceDialog with" << seqCount << "sequences";

    if (dlg.exec() != QDialog::Accepted) {
        qDebug() << "[runSequenceDialog] Dialog cancelled";
        return;
    }

    qDebug() << "[runSequenceDialog] Dialog completed with selection";

    if (dlg.selectedMode() == RunTemplateOrSequenceDialog::RunSequence) {
        qDebug() << "[runSequenceDialog] Running individual sequence mode";
        const auto& currentSequences = m_mw->m_seqMgr.sequences();
        QString path = dlg.selectedSequencePath();
        if (!currentSequences.contains(path)) {
            QMessageBox::warning(m_mw, "Ошибка", "Файл последовательности не найден.");
            return;
        }

        m_mw->m_currentSeq = currentSequences.value(path);
        m_mw->m_currentStepIdx = 0;
        m_mw->m_sequenceMode = true;
        m_mw->m_currentSeqPath = path;
        m_mw->m_currentSeqId = m_mw->m_contextManager->startCommandSequence(m_mw->m_currentSeq.name(), m_mw->m_currentSeq.description());
        resetSequenceStabilityState();
        m_mw->updateSequenceGraphHighlight();
        qDebug() << "[runSequenceDialog] About to start task timer for sequence execution";
        m_mw->m_statusController->startTask();
        qDebug() << "[runSequenceDialog] Starting sequence:" << m_mw->m_currentSeq.name();
        runNextStep();
    } else {
        qDebug() << "[runSequenceDialog] Running template mode";
        QString templatePath = dlg.selectedTemplatePath();
        QString sequencePath = dlg.selectedDefaultSequencePath();

        if (sequencePath.isEmpty() || !QFile::exists(sequencePath)) {
            QMessageBox::warning(m_mw, "Ошибка", "Выбранная последовательность для шаблона не найдена.");
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
            QMessageBox::warning(m_mw, "Ошибка", "Не удалось загрузить выбранную последовательность.");
            return;
        }

        m_mw->m_currentSeq = seq;
        m_mw->m_currentStepIdx = 0;
        m_mw->m_sequenceMode = true;
        m_mw->m_currentSeqPath = sequencePath;
        if (m_mw->m_contextManager)
            m_mw->m_currentSeqId = m_mw->m_contextManager->startCommandSequence(m_mw->m_currentSeq.name(), m_mw->m_currentSeq.description());
        else
            m_mw->m_currentSeqId.clear();
        m_mw->m_currentTemplatePath = templatePath;
        resetSequenceStabilityState();
        m_mw->updateSequenceGraphHighlight();
        qDebug() << "[runSequenceDialog] About to start task timer for template execution";
        m_mw->m_statusController->startTask();
        runNextStep();
    }
}

void SequenceRunner::runNextStep()
{
    qDebug() << "[runNextStep] m_sequenceMode:" << m_mw->m_sequenceMode << "m_currentStepIdx:" << m_mw->m_currentStepIdx;
    if (!m_mw->m_sequenceMode) return;

    if (m_mw->m_stopRequested) {
        m_mw->m_stopRequested = false;
        m_mw->m_sequenceMode = false;
        m_mw->m_currentSeqId.clear();
        m_mw->updateSequenceGraphHighlight();

        QMessageBox::information(m_mw, "Остановлено",
                                 "Выполнение шаблона остановлено пользователем.");
        return;
    }

    if (m_mw->m_currentStepIdx < 0 ||
            m_mw->m_currentStepIdx >= m_mw->m_currentSeq.steps().size()) {
        if (!m_resultShownInCurrentRun) {
            autoFinalizeSequenceWithResult(m_mw->tr("последовательность завершилась без SHOW_RESULT"), QList<Command*>());
            return;
        }
        QMessageBox::information(m_mw, "Готово",
                                 "Последовательность выполнена полностью.");
        if (!m_mw->m_currentSeqId.isEmpty() && m_mw->m_contextManager) {
            m_mw->m_contextManager->endCommandSequence(m_mw->m_currentSeqId);
        }
        m_mw->m_sequenceMode = false;
        m_mw->m_currentSeqId.clear();
        m_mw->m_statusController->stopTask();
        m_mw->updateSequenceGraphHighlight();
        return;
    }

    const PromptStep &step = m_mw->m_currentSeq.steps()[m_mw->m_currentStepIdx];
    PromptResolver::ResolvedPrompt resolved = PromptResolver::resolve(step.tmpl(), step, m_mw);
    qDebug() << "[runNextStep] systemPrompt snippet:" << resolved.systemPrompt;
    qDebug() << "[runNextStep] userPrompt:" << resolved.userPrompt;

    {
        int tokens = m_mw->m_model.estimateTokens(resolved.systemPrompt + "\n" + resolved.userPrompt);
        m_mw->log(QString("[Шаг %1] Контекст ≈ %2 токенов").arg(m_mw->m_currentStepIdx+1).arg(tokens));
    }
    m_mw->log(QString("\n--- Шаг %1 ------------------------------------------------")
        .arg(m_mw->m_currentStepIdx+1));
    m_mw->log(resolved.systemPrompt);
    if (!resolved.userPrompt.isEmpty())
        m_mw->log(QString("USER_QUERY:\n%1").arg(resolved.userPrompt));

    if (!resolved.userPrompt.isEmpty()) {
        if (m_mw->m_lastUserMessages.isEmpty() || m_mw->m_lastUserMessages.last() != resolved.userPrompt) {
            m_mw->m_history.push_back(qMakePair(QString("user"), resolved.userPrompt));
            m_mw->m_lastUserMessages.append(resolved.userPrompt);
            while (m_mw->m_lastUserMessages.size() > MainWindow::MAX_USER_MESSAGES) {
                m_mw->m_lastUserMessages.removeFirst();
            }
            qDebug() << "[runNextStep] Added user prompt to LAST_USER_MESSAGES storage. Current count:" << m_mw->m_lastUserMessages.size();
        } else {
            qDebug() << "[runNextStep] Skipping duplicate user prompt in LAST_USER_MESSAGES storage";
        }
    }

    m_mw->m_streamBuf.clear();

    qDebug() << "[DEBUG SEND TO LLM] - USER PROMPT:" << resolved.userPrompt;
    qDebug() << "[DEBUG SEND TO LLM] - SYSTEM PROMPT (first 500 chars):" << resolved.systemPrompt.left(500);
    qDebug() << "[DEBUG SEND TO LLM] - SYSTEM PROMPT LENGTH:" << resolved.systemPrompt.length();

    m_mw->m_currentPrompt = resolved.systemPrompt + "\n" + resolved.userPrompt;

    if (m_mw->m_agentRepairStepIdx != m_mw->m_currentStepIdx || m_mw->m_agentRepairSeqId != m_mw->m_currentSeqId) {
        m_mw->m_agentRepairCountThisStep = 0;
        m_mw->m_agentRepairStepIdx = m_mw->m_currentStepIdx;
        m_mw->m_agentRepairSeqId = m_mw->m_currentSeqId;
    }

    if (m_mw->m_useAgentPipeline && ensureAgentModelConfigured()) {
        QVariantMap draftOpts;
        draftOpts.insert("temperature", 0.5);
        draftOpts.insert("num_predict", 600);
        draftOpts.insert("timeoutMs", 60000);

        const QString draftSystem =
            QStringLiteral(
                "You are a senior planner.\n"
                "Return a short plan (5-10 bullets) and, if needed, 1-3 clarifying questions.\n"
                "Do NOT output JSON commands in this step.\n");

        const QString rawDraft = m_mw->m_agentModel.generateWithSystem(resolved.userPrompt, draftSystem, draftOpts).trimmed();
        if (!rawDraft.isEmpty())
            m_mw->appendConsoleContext(QStringLiteral("AGENT_RAW_DRAFT"), rawDraft, QString(), 0);

        QVariantMap compileOpts;
        compileOpts.insert("temperature", 0.1);
        compileOpts.insert("num_predict", 1400);
        compileOpts.insert("timeoutMs", 90000);

        QString compileSystem = resolved.systemPrompt;
        if (!rawDraft.isEmpty()) {
            compileSystem += QStringLiteral("\n\n[RAW_DRAFT]\n") + rawDraft + QStringLiteral("\n\n");
        }
        compileSystem += QStringLiteral("\n\nIMPORTANT: Output ONLY a JSON array of commands. No extra text.\n");

        const QString compiled = m_mw->m_agentModel.generateWithSystem(resolved.userPrompt, compileSystem, compileOpts);
        QTimer::singleShot(0, m_mw, [this, compiled]() {
            m_mw->m_streamBuf = compiled;
            handleAssistantFinished();
        });
        return;
    }

    if (!m_mw->m_activeImagesBase64.isEmpty()) {
        m_mw->m_model.startStreamWithSystemAndImages(resolved.userPrompt, resolved.systemPrompt, m_mw->m_activeImagesBase64, QVariantMap());
    } else {
        m_mw->m_model.startStreamWithSystem(resolved.userPrompt, resolved.systemPrompt);
    }
}

void SequenceRunner::handleAssistantFinished()
{
    if (m_mw->m_stopRequested) {
        m_mw->m_stopRequested = false;
        m_mw->m_sequenceMode = false;
        m_mw->m_currentSeqId.clear();
        m_mw->updateSequenceGraphHighlight();

        QMessageBox::information(m_mw, "Остановлено",
                                 "Выполнение шаблона остановлено пользователем.");
        m_mw->m_statusController->stopTask();
        qDeleteAll(CommandParser::parse(m_mw->m_streamBuf.toUtf8()));
        return;
    }

    QByteArray fullResp = m_mw->m_streamBuf.toUtf8();
    QByteArray jsonResp = m_mw->extractJsonPart(fullResp);

    if (jsonResp.isEmpty() && m_mw->m_sequenceMode &&
        m_mw->m_currentStepIdx >= 0 && m_mw->m_currentStepIdx < m_mw->m_currentSeq.steps().size()) {
        const PromptStep &step = m_mw->m_currentSeq.steps()[m_mw->m_currentStepIdx];
        const QByteArray repaired = repairToJsonArrayIfNeeded(fullResp, step);
        if (!repaired.isEmpty()) {
            jsonResp = repaired;
        }
    }

    const bool usedRaw = jsonResp.isEmpty();
    const QByteArray payload = usedRaw ? fullResp : jsonResp;
    if (usedRaw) {
        m_mw->appendConsoleContext("LLM RAW OUTPUT", QString::fromUtf8(fullResp), QString(), -1);
        m_mw->log("LLM ответ сохранён как контекст: не удалось извлечь JSON. Пытаемся распарсить сырой ответ.");
    }

    m_mw->m_history.push_back(qMakePair(QString("assistant"), QString::fromUtf8(payload)));
    m_mw->m_lastAssistantMessage = QString::fromUtf8(payload);

    // Check if the LLM specified to open a new sequence
    QString nextSeqName = PromptResolver::getNextSequence();
    if (!nextSeqName.isEmpty()) {
        m_mw->log("DEBUG: NEXT_SEQUENCE requested, searching for: " + nextSeqName);
        m_mw->refreshAllSequences();

        const QMap<QString, PromptSequence>& allSequences = m_mw->m_seqMgr.sequences();
        PromptSequence foundSeq;
        QString foundPath;

        for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
            if (it.value().name() == nextSeqName) {
                foundSeq = it.value();
                foundPath = it.key();
                m_mw->log("Found sequence by name: " + nextSeqName + " at " + foundPath);
                break;
            }
        }

        if (foundPath.isEmpty()) {
            for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
                QFileInfo fileInfo(it.key());
                if (fileInfo.fileName() == nextSeqName) {
                    foundSeq = it.value();
                    foundPath = it.key();
                    m_mw->log("Found sequence by filename: " + nextSeqName + " at " + foundPath);
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
                    m_mw->log("Found sequence by expected filename: " + expectedFileName + " at " + foundPath);
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
                    m_mw->log("Found sequence by basename match: " + nextSeqName + " at " + foundPath);
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
                    m_mw->log("Found sequence by basename + .seq match: " + nextSeqName + ".seq" + " at " + foundPath);
                    break;
                }
            }
        }

        if (foundPath.isEmpty() && !m_mw->m_currentSeqPath.isEmpty()) {
            QFileInfo currentSeqInfo(m_mw->m_currentSeqPath);
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
                            m_mw->log(QString("Found sequence in template directory: %1").arg(sequenceFilePath));
                            break;
                        }
                        seqFile.close();
                    }
                }
                if (!foundPath.isEmpty()) break;
            }
        }

        if (!foundPath.isEmpty()) {
            m_mw->m_currentSeq = foundSeq;
            m_mw->m_currentStepIdx = 0;
            m_mw->m_sequenceMode = true;
            m_mw->m_currentSeqPath = foundPath;
            addSequenceTransitionToHistory(nextSeqName);
            m_mw->updateSequenceGraphHighlight();
            PromptResolver::clearNextSequence();
            QTimer::singleShot(200, m_mw, [this]() { runNextStep(); });
            qDeleteAll(CommandParser::parse(payload));
            return;
        }

        m_mw->log("Указанная последовательность не найдена: " + nextSeqName);
        m_mw->log("DEBUG: NEXT_SEQUENCE - Available sequences debugging:");
        m_mw->log("  Total sequences in manager: " + QString::number(allSequences.size()));
        m_mw->log("  Searched sequence name: '" + nextSeqName + "'");
        m_mw->log("  Available sequence names and paths:");
        for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
            QString seqName = it.value().name();
            QString filePath = it.key();
            QFileInfo fileInfo(filePath);
            m_mw->log("    - Sequence name: '" + seqName + "'");
            m_mw->log("    - File name: '" + fileInfo.fileName() + "'");
            m_mw->log("    - Base name: '" + fileInfo.baseName() + "'");
            m_mw->log("    - File path: " + filePath);
        }
        PromptResolver::clearNextSequence();
    }

    QVector<Command*> cmds = CommandParser::parse(payload);
    if (cmds.isEmpty()) {
        if (!usedRaw)
            m_mw->appendConsoleContext("LLM RAW OUTPUT", QString::fromUtf8(payload), QString(), -1);
        m_mw->log("LLM не вернул команд. Сырой ответ добавлен в контекст.");

        if (m_mw->m_sequenceMode) {
            ++m_batchesWithoutResult;
            if (m_batchesWithoutResult >= 7 && !m_resultShownInCurrentRun) {
                autoFinalizeSequenceWithResult(m_mw->tr("LLM несколько шагов подряд не возвращает команды"), QList<Command*>());
                return;
            }
            if (m_mw->m_stopRequested) {
                m_mw->m_stopRequested = false;
                m_mw->m_sequenceMode = false;
                m_mw->m_currentSeqId.clear();
                m_mw->updateSequenceGraphHighlight();

                QMessageBox::information(m_mw, "Остановлено",
                                         "Выполнение шаблона остановлено пользователем.");
                return;
            }
            ++m_mw->m_currentStepIdx;
            QTimer::singleShot(200, m_mw, [this]() { runNextStep(); });
        } else {
            retryCurrentSequenceStep();
        }
        return;
    }

    QList<Command*> toRun;
    if (m_mw->confirmModeCombo->currentData().toString() == "auto") {
        toRun = cmds.toList();
        m_mw->log(QString("Команды запускаются без подтверждения (режим: %1).").arg(m_mw->confirmModeCombo->currentText()));
    } else {
        auto dlg = new CommandConfirmDialog(cmds.toList(), m_mw, true);
        if (dlg->exec() != QDialog::Accepted) {
            m_mw->log("Пользователь отклонил команды");
            qDeleteAll(cmds);
            delete dlg;
            return;
        }
        toRun = dlg->confirmed();
        delete dlg;
    }

    const bool hasShowResultCommand = std::any_of(toRun.begin(), toRun.end(), [](Command* c) {
        return c && c->name() == QStringLiteral("SHOW_RESULT");
    });
    const bool readOnlyBatch = isReadOnlyCommandBatch(toRun);
    const QString batchSignature = commandBatchSignature(toRun);

    bool shouldAutoFinalizeBeforeTransition = false;
    QString autoFinalizeReason;
    if (m_mw->m_sequenceMode) {
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
                autoFinalizeReason = m_mw->tr("слишком много шагов без финального ответа");
            } else if (readOnlyBatch && m_sameBatchRepeatCount >= 3) {
                shouldAutoFinalizeBeforeTransition = true;
                autoFinalizeReason = m_mw->tr("повторяющиеся команды без прогресса");
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
            const QString ans = QInputDialog::getText(m_mw,
                                                      m_mw->tr("Ответьте на вопрос нейросети"),
                                                      question,
                                                      QLineEdit::Normal,
                                                      QString(),
                                                      &ok);
            if (ok) {
                m_mw->m_history.push_back(qMakePair(QString("user"), ans));
                m_mw->m_lastUserMessages.append(ans);
                while (m_mw->m_lastUserMessages.size() > MainWindow::MAX_USER_MESSAGES) {
                    m_mw->m_lastUserMessages.removeFirst();
                }
            }
        } else if (c->name() == "OPEN_FILE_REGION" || c->name() == "OPEN_FILE") {
            m_mw->handleOpenCommand(c);
        } else if (c->name() == "SHOW_RESULT") {
            QString title = c->params().value("title").toString();
            if (title.isEmpty())
                title = m_mw->tr("Ответ от %1:\n\n").arg(m_mw->m_model.selectedModel());
            QString content = c->params().value("content").toString() + "\n\n\n";
            if (content.isEmpty())
                content = m_mw->tr("SHOW_RESULT вызван без текста.\n\n\n");
            appendResultToUi(title, content);
            m_resultShownInCurrentRun = true;
            m_batchesWithoutResult = 0;
            m_sameBatchRepeatCount = 0;
            m_lastBatchSignature.clear();

            if (m_mw->m_ttsController && m_mw->m_ttsController->isEnabled() && m_mw->m_ttsController->runner() && m_mw->m_ttsController->runner()->isReady()) {
                QString textToSpeak = c->params().value("content").toString();
                textToSpeak.replace("\\n", " ").replace("\\r", "").replace("\n", " ");
                if (textToSpeak.length() > 1000) {
                    textToSpeak = textToSpeak.left(1000) + "...";
                }
                if (!textToSpeak.isEmpty() && textToSpeak.length() > 5) {
                    m_mw->m_ttsController->speak(textToSpeak);
                }
            }

            if (m_mw->m_contextManager) {
                m_mw->addCommandToContext(c, c->getResult());
            }
        } else if (c->name() == "REASONING") {
            QString reasoning = c->params().value("reasoning").toString();
            if (reasoning.isEmpty())
                reasoning = c->params().value("thought_process").toString();
            if (reasoning.isEmpty())
                reasoning = c->params().value("reasoning_text").toString();

            if (!reasoning.isEmpty()) {
                if (m_mw->m_resultDisplayWidget) {
                    QString formattedReasoning = reasoning;
                    auto metadata = m_mw->collectContextMetadataSince(m_mw->m_lastQuestionTimestamp);
                    m_mw->m_resultDisplayWidget->appendResult("", formattedReasoning,
                                                    ResultDisplayWidget::EntryType::Reasoning,
                                                    metadata);
                } else {
                    m_mw->m_history.push_back(qMakePair(QString("system"), reasoning));
                }
            }

            if (m_mw->m_contextManager) {
                m_mw->addCommandToContext(c, c->getResult());
            }
        } else if (c->name() == "CONTEXT_SHOW") {
            m_mw->m_contextCache.rebuildContextCache();
            int maxChars = c->params().value("maxChars").toInt(8000);
            if (maxChars <= 0) maxChars = 0;
            QString ctx = m_mw->m_contextCache.contextContent();
            if (maxChars > 0 && ctx.size() > maxChars) {
                ctx = ctx.left(maxChars) + "\n[...truncated...]";
            }

            const QString result = QString("CONTEXT_SHOW\n[context]\n%1").arg(ctx.trimmed());

            if (m_mw->m_resultDisplayWidget) {
                auto metadata = m_mw->collectContextMetadataSince(m_mw->m_lastQuestionTimestamp);
                m_mw->m_resultDisplayWidget->appendResult(m_mw->tr("Context"), result,
                                                    ResultDisplayWidget::EntryType::Default,
                                                    metadata);
            } else {
                m_mw->appendExternalContext(QString("==== CONTEXT_SHOW ====\n%1").arg(ctx));
            }

            m_mw->m_history.push_back(qMakePair(QString("system"), result));
            if (m_mw->m_contextManager) {
                m_mw->addCommandToContext(c, result);
            }
        } else if (c->name() == "CONTEXT_CLEAR") {
            const QString scope = c->params().value("scope").toString().trimmed().toLower();

            m_mw->m_contextCache.clearAll();
            m_mw->m_fileToContextCommands.clear();
            if (m_mw->m_fileWatcher)
                m_mw->m_fileWatcher->removePaths(m_mw->m_fileWatcher->files());

            m_mw->m_contextCache.clearConsoleLogs();
            m_mw->m_contextCache.clearExternalSources();
            m_mw->m_shellCommandOutputs.clear();
            m_mw->m_pythonScriptOutputs.clear();

            if (m_mw->m_contextManager) {
                if (scope == "all")
                    m_mw->m_contextManager->clearAll();
                else
                    m_mw->m_contextManager->clearActiveContextOnly();
            }

            m_mw->m_contextCache.rebuildContextCache();

            const QString result = QString("CONTEXT_CLEAR\n[scope]\n%1\n[status]\nOK").arg(scope.isEmpty() ? "active" : scope);
            m_mw->m_history.push_back(qMakePair(QString("system"), result));
            if (m_mw->m_contextManager) {
                m_mw->addCommandToContext(c, result);
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
        const QMap<QString, PromptSequence>& allSequences = m_mw->m_seqMgr.sequences();
        for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
            if (it.value().name() == sequenceToCall) {
                foundSeq = it.value();
                foundPath = it.key();
                break;
            }
        }

        if (foundPath.isEmpty() && !m_mw->m_currentSeqPath.isEmpty()) {
            QFileInfo currentSeqInfo(m_mw->m_currentSeqPath);
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
                for (int i = 0; i < m_mw->m_currentStepIdx; ++i) {
                    if (i < m_mw->m_currentSeq.steps().size()) {
                        QString stepDesc = m_mw->m_currentSeq.steps()[i].tmpl().section('\n', 0, 0).trimmed();
                        if (stepDesc.length() > 80) stepDesc = stepDesc.left(80) + "...";
                        completedStepsSummary << QString("Шаг %1: %2").arg(i+1).arg(stepDesc);
                    }
                }

                if (!m_mw->m_currentSeqId.isEmpty() && m_mw->m_contextManager) {
                    m_mw->m_contextManager->endCommandSequence(m_mw->m_currentSeqId);
                }

                m_mw->m_currentSeq = foundSeq;
                m_mw->m_currentStepIdx = 0;
                m_mw->m_sequenceMode = true;
                m_mw->m_currentSeqId = m_mw->m_contextManager->startCommandSequence(m_mw->m_currentSeq.name(), m_mw->m_currentSeq.description());
                m_mw->m_currentSeqPath = foundPath;

                if (!completedStepsSummary.isEmpty()) {
                    QString summaryText = QString("Предыдущая последовательность выполнила: %1").arg(completedStepsSummary.join("; "));
                    m_mw->m_history.push_back(qMakePair(QString("system"), summaryText));
                }

                addSequenceTransitionToHistory(sequenceToCall);
                m_mw->updateSequenceGraphHighlight();
                qDeleteAll(toRun);
                QTimer::singleShot(200, m_mw, [this]() { runNextStep(); });
                return;
            } else {
                if (!m_mw->m_currentSeqId.isEmpty() && m_mw->m_contextManager) {
                    m_mw->m_contextManager->endCommandSequence(m_mw->m_currentSeqId);
                }

                m_mw->m_currentSeq = foundSeq;
                m_mw->m_currentStepIdx = 0;
                m_mw->m_sequenceMode = true;
                m_mw->m_currentSeqId = m_mw->m_contextManager->startCommandSequence(m_mw->m_currentSeq.name(), m_mw->m_currentSeq.description());
                m_mw->m_currentSeqPath = foundPath;

                addSequenceTransitionToHistory(sequenceToCall);
                m_mw->updateSequenceGraphHighlight();
                qDeleteAll(toRun);
                QTimer::singleShot(200, m_mw, [this]() { runNextStep(); });
                return;
            }
        }

        m_mw->log("Указанная последовательность не найдена: " + sequenceToCall);
        const QMap<QString, PromptSequence>& allSequences_debug = m_mw->m_seqMgr.sequences();
        m_mw->log("DEBUG: CALL_SEQUENCE - Available sequences debugging:");
        m_mw->log("  Total sequences in main manager: " + QString::number(allSequences_debug.size()));
        m_mw->log("  Current sequence path: '" + m_mw->m_currentSeqPath + "'");
        if (!m_mw->m_currentSeqPath.isEmpty()) {
            QFileInfo currentSeqInfo(m_mw->m_currentSeqPath);
            QDir templateDir = currentSeqInfo.absoluteDir();
            m_mw->log("  Current sequence directory: " + templateDir.absolutePath());
            QStringList filters = {"*.seq.json", "*.seq", "*.json"};
            QFileInfoList files = templateDir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);
            m_mw->log("  Sequence files in current directory (" + QString::number(files.size()) + "):");
            for (const QFileInfo& fileInfo : files) {
                m_mw->log("    - File: " + fileInfo.fileName());
            }
        }
        m_mw->log("  Searched sequence name: '" + sequenceToCall + "'");
        m_mw->log("  Available sequence names and paths in main manager:");
        for (auto it = allSequences_debug.constBegin(); it != allSequences_debug.constEnd(); ++it) {
            QString seqName = it.value().name();
            QString filePath = it.key();
            QFileInfo fileInfo(filePath);
            m_mw->log("    - Sequence name: '" + seqName + "'");
            m_mw->log("    - File name: '" + fileInfo.fileName() + "'");
            m_mw->log("    - Base name: '" + fileInfo.baseName() + "'");
            m_mw->log("    - File path: " + filePath);
        }
    }

    // OPEN_SEQUENCE handling
    if (hasOpenSequenceCommand && !openSequenceName.isEmpty()) {
        PromptSequence foundSeq2;
        QString foundPath2;
        const QMap<QString, PromptSequence>& allSequences = m_mw->m_seqMgr.sequences();
        for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
            if (it.value().name() == openSequenceName) {
                foundSeq2 = it.value();
                foundPath2 = it.key();
                break;
            }
        }

        if (foundPath2.isEmpty() && !m_mw->m_currentSeqPath.isEmpty()) {
            QFileInfo currentSeqInfo(m_mw->m_currentSeqPath);
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
            if (!m_mw->m_currentSeqId.isEmpty() && m_mw->m_contextManager) {
                m_mw->m_contextManager->endCommandSequence(m_mw->m_currentSeqId);
            }

            m_mw->m_currentSeq = foundSeq2;
            m_mw->m_currentStepIdx = 0;
            m_mw->m_sequenceMode = true;
            m_mw->m_currentSeqId = m_mw->m_contextManager->startCommandSequence(m_mw->m_currentSeq.name(), m_mw->m_currentSeq.description());
            m_mw->m_currentSeqPath = foundPath2;

            addSequenceTransitionToHistory(openSequenceName);
            m_mw->updateSequenceGraphHighlight();
            qDeleteAll(toRun);
            QTimer::singleShot(200, m_mw, [this]() { runNextStep(); });
            return;
        }

        m_mw->log("Указанная последовательность не найдена: " + openSequenceName);
        const QMap<QString, PromptSequence>& allSequences_debug2 = m_mw->m_seqMgr.sequences();
        m_mw->log("DEBUG: OPEN_SEQUENCE - Available sequences debugging:");
        m_mw->log("  Total sequences in main manager: " + QString::number(allSequences_debug2.size()));
        m_mw->log("  Current sequence path: '" + m_mw->m_currentSeqPath + "'");
        if (!m_mw->m_currentSeqPath.isEmpty()) {
            QFileInfo currentSeqInfo(m_mw->m_currentSeqPath);
            QDir templateDir = currentSeqInfo.absoluteDir();
            m_mw->log("  Current sequence directory: " + templateDir.absolutePath());
            QStringList filters = {"*.seq.json", "*.seq", "*.json"};
            QFileInfoList files = templateDir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);
            m_mw->log("  Sequence files in current directory (" + QString::number(files.size()) + "):");
            for (const QFileInfo& fileInfo : files) {
                m_mw->log("    - File: " + fileInfo.fileName());
            }
        }
        m_mw->log("  Searched sequence name: '" + openSequenceName + "'");
        m_mw->log("  Available sequence names and paths in main manager:");
        for (auto it = allSequences_debug2.constBegin(); it != allSequences_debug2.constEnd(); ++it) {
            QString seqName = it.value().name();
            QString filePath = it.key();
            QFileInfo fileInfo(filePath);
            m_mw->log("    - Sequence name: '" + seqName + "'");
            m_mw->log("    - File name: '" + fileInfo.fileName() + "'");
            m_mw->log("    - Base name: '" + fileInfo.baseName() + "'");
            m_mw->log("    - File path: " + filePath);
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
        m_mw->m_prevSteps << QString("Шаг %1: %2").arg(m_mw->m_currentStepIdx + 1).arg(appliedSummary.join("; "));
    }

    QString err;
    auto logConsoleResult = [&](Command* cmd, const QString& errorMessage = QString()){
        QString result = cmd->getResult();

        if (!result.isEmpty()) {
            QString fullTitle = result.split('\n').first();
            m_mw->appendConsoleContext(fullTitle, result, QString(), errorMessage.isEmpty() ? (cmd->wasSuccessful() ? 0 : 1) : 1);

            m_mw->m_history.push_back(qMakePair(QString("system"), result));
        }

        if (m_mw->m_contextManager) {
            m_mw->addCommandToContext(cmd, result);
        }
    };

    QList<Command*> pendingEdits;
    bool applyFailed = false;
    const QDateTime execStartedAt = QDateTime::currentDateTime();
    OllamaAdapter* execModel = &m_mw->m_model;
    if (m_mw->m_useAgentPipeline && ensureAgentModelConfigured())
        execModel = &m_mw->m_agentModel;
    for (Command* c : toRun) {
        const QString name = c->name();
        if (name == "ASK_USER" || name == "OPEN_FILE" || name == "OPEN_FILE_REGION" || name == "SHOW_RESULT" || name == "REASONING"
            || name == "CONTEXT_SHOW" || name == "CONTEXT_CLEAR")
            continue;
        if (name == "EDIT_IN_RANGE") {
            pendingEdits << c;
            continue;
        }
        if (!c->applyWithLLM(&m_mw->m_dev_manager, execModel, &err)) {
            c->setExecutionResult(false, err);
            logConsoleResult(c, err);
            QMessageBox::warning(m_mw, m_mw->tr("Ошибка выполнения"), err);
            applyFailed = true;
            break;
        }
        c->setExecutionResult(true);
        m_mw->handlePostApplyContextCleanup(c);
        logConsoleResult(c, QString());
        m_mw->handleSearchResults(c);
    }

    if (!applyFailed && !pendingEdits.isEmpty()) {
        if (!m_mw->applyGroupedEditInRange(pendingEdits, &err)) {
            QMessageBox::warning(m_mw, m_mw->tr("Ошибка выполнения"), err);
        } else {
            for (Command* c : pendingEdits) {
                m_mw->handlePostApplyContextCleanup(c);
                m_mw->handleSearchResults(c);
            }
        }
    }

    m_mw->rescan();

    if (applyFailed && !m_mw->m_sequenceMode) {
        retryCurrentSequenceStep();
    } else if (m_mw->m_sequenceMode) {
        if (m_mw->m_stopRequested) {
            m_mw->m_stopRequested = false;
            m_mw->m_sequenceMode = false;
            m_mw->m_currentSeqId.clear();
            m_mw->updateSequenceGraphHighlight();

            QMessageBox::information(m_mw, "Остановлено",
                                     "Выполнение шаблона остановлено пользователем.");
            qDeleteAll(toRun);
            return;
        }

        const PromptStep &step = m_mw->m_currentSeq.steps()[m_mw->m_currentStepIdx];
        if (maybeAutoRepairAfterExecution(step, execStartedAt, toRun, applyFailed, err)) {
            qDeleteAll(toRun);
            return;
        }
        const bool finishingCurrentStep = !step.loop() && (m_mw->m_currentStepIdx + 1 >= m_mw->m_currentSeq.steps().size());
        if (finishingCurrentStep && !hasShowResultCommand && !m_resultShownInCurrentRun) {
            autoFinalizeSequenceWithResult(m_mw->tr("финальный шаг не вернул SHOW_RESULT"), toRun);
            qDeleteAll(toRun);
            return;
        }
        if (step.loop()) {
            QTimer::singleShot(200, m_mw, [this]() { runNextStep(); });
        } else {
            ++m_mw->m_currentStepIdx;
            QTimer::singleShot(200, m_mw, [this]() { runNextStep(); });
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
    if (m_mw->m_resultDisplayWidget) {
        auto metadata = m_mw->collectContextMetadataSince(m_mw->m_lastQuestionTimestamp);
        m_mw->m_resultDisplayWidget->appendResult(title, content,
                                            ResultDisplayWidget::EntryType::Default,
                                            metadata);
    } else {
        m_mw->appendExternalContext(QString("==== SHOW_RESULT ====\n%1").arg(content));
    }
}

void SequenceRunner::autoFinalizeSequenceWithResult(const QString& reason, const QList<Command*>& recentCommands)
{
    QStringList lines;
    Q_UNUSED(reason);
    lines << m_mw->tr("Я завершил выполнение автоматически, чтобы не допустить зацикливания.");
    if (!m_mw->m_lastUserMessage.trimmed().isEmpty())
        lines << m_mw->tr("Запрос: %1").arg(m_mw->m_lastUserMessage.trimmed());

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
        lines << m_mw->tr("Найденные источники:");
        for (const QString& s : sourceLines)
            lines << QString("- %1").arg(s);
    } else if (!commandNames.isEmpty()) {
        lines << m_mw->tr("Последние выполненные команды: %1").arg(commandNames.join(", "));
    }

    lines << m_mw->tr("Если нужно, сформулируйте запрос чуть точнее: период, регион, источники.");
    const QString content = lines.join(QStringLiteral("\n\n")) + QStringLiteral("\n\n\n");
    appendResultToUi(m_mw->tr("Ответ нейросети"), content);
    m_resultShownInCurrentRun = true;

    if (!m_mw->m_currentSeqId.isEmpty() && m_mw->m_contextManager)
        m_mw->m_contextManager->endCommandSequence(m_mw->m_currentSeqId);
    m_mw->m_currentSeqId.clear();
    m_mw->m_sequenceMode = false;
    m_mw->updateSequenceGraphHighlight();
    m_mw->m_statusController->stopTask();
}

void SequenceRunner::retryCurrentSequenceStep()
{
    if (!m_mw->m_sequenceMode)
        return;

    if (m_mw->m_stopRequested) {
        m_mw->m_stopRequested = false;
        m_mw->m_sequenceMode = false;
        m_mw->m_currentSeqId.clear();
        m_mw->updateSequenceGraphHighlight();

        QMessageBox::information(m_mw, "Остановлено",
                                 "Выполнение шаблона остановлено пользователем.");
        return;
    }

    QTimer::singleShot(200, m_mw, [this]() { runNextStep(); });
}

bool SequenceRunner::ensureAgentModelConfigured()
{
    if (!m_mw->m_useAgentPipeline)
        return false;

    if (m_mw->m_agentModel.selectedModel().isEmpty()) {
        m_mw->m_agentModel.configure(QStringLiteral("gpt-oss:20b"), QVariantMap());
    }

    QString err;
    if (!m_mw->m_agentModel.isAvailable(&err)) {
        m_mw->log(QString("Agent model unavailable (%1). Falling back to selected model.").arg(err));
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
    const QByteArray extracted = m_mw->extractJsonPart(rawText);
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

    const QString repaired = m_mw->m_agentModel.generateWithSystem(QString::fromUtf8(rawText), systemPrompt, opts);
    const QByteArray repairedUtf8 = repaired.toUtf8();
    const QByteArray repairedJson = m_mw->extractJsonPart(repairedUtf8);
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
            const QString p = normalizePathForProject(raw, m_mw->m_projectRoot);
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
    if (!m_mw->m_sequenceMode)
        return false;
    if (!m_mw->m_useAgentPipeline)
        return false;
    if (m_mw->m_inAgentAutoRepair)
        return false;

    if (m_mw->m_agentRepairStepIdx != m_mw->m_currentStepIdx || m_mw->m_agentRepairSeqId != m_mw->m_currentSeqId) {
        m_mw->m_agentRepairCountThisStep = 0;
        m_mw->m_agentRepairStepIdx = m_mw->m_currentStepIdx;
        m_mw->m_agentRepairSeqId = m_mw->m_currentSeqId;
    }

    if (m_mw->m_agentRepairCountThisStep >= m_mw->m_agentMaxRepairsPerStep)
        return false;

    const bool implIntent = isImplementationIntentText(m_mw->m_lastUserMessage);
    const bool changed = didAnyAffectedPathChangeSince(execStartedAt, executedCommands);
    const bool needRepair = applyFailed || (implIntent && !changed);
    if (!needRepair)
        return false;

    if (!ensureAgentModelConfigured())
        return false;

    const QStringList allowedDocs = buildAllowedCommandsDocsCompact(step);
    const QString ctx = m_mw->m_contextManager ? m_mw->m_contextManager->generateContextText() : QString();
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

    QString userPrompt = QStringLiteral("USER REQUEST:\n%1\n").arg(m_mw->m_lastUserMessage);
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

    const QString repair = m_mw->m_agentModel.generateWithSystem(userPrompt, systemPrompt, opts);
    const QByteArray repairedJson = m_mw->extractJsonPart(repair.toUtf8());
    if (repairedJson.isEmpty())
        return false;

    m_mw->m_agentRepairCountThisStep++;
    m_mw->m_inAgentAutoRepair = true;
    m_mw->appendConsoleContext(QStringLiteral("AGENT_REPAIR_DRAFT"), repair, QString(), 0);

    QTimer::singleShot(0, m_mw, [this, repairedJson]() {
        m_mw->m_streamBuf = QString::fromUtf8(repairedJson);
        handleAssistantFinished();
        m_mw->m_inAgentAutoRepair = false;
    });
    return true;
}

void SequenceRunner::runNextSequence()
{
    if (!m_mw->m_sequenceMode) return;
    if (m_mw->m_currentStepIdx < 0 ||
            m_mw->m_currentStepIdx >= m_mw->m_currentSeq.steps().size()) {
        QMessageBox::information(m_mw, "Готово",
                                 "Последовательность выполнена полностью.");
        if (!m_mw->m_currentSeqId.isEmpty() && m_mw->m_contextManager) {
            m_mw->m_contextManager->endCommandSequence(m_mw->m_currentSeqId);
        }
        m_mw->m_sequenceMode = false;
        m_mw->m_currentSeqId.clear();
        m_mw->m_statusController->stopTask();
        m_mw->updateSequenceGraphHighlight();
        return;
    }

    const PromptStep &step = m_mw->m_currentSeq.steps()[m_mw->m_currentStepIdx];
    PromptResolver::ResolvedPrompt resolved = PromptResolver::resolve(step.tmpl(), step, m_mw);
    qDebug() << "[runNextSequence] systemPrompt snippet:" << resolved.systemPrompt.left(200);
    qDebug() << "[runNextSequence] userPrompt:" << resolved.userPrompt;

    {
        int tokens = m_mw->m_model.estimateTokens(resolved.systemPrompt + "\n" + resolved.userPrompt);
        m_mw->log(QString("[Шаг %1] Контекст ≈ %2 токенов").arg(m_mw->m_currentStepIdx+1).arg(tokens));
    }
    m_mw->log(QString("\n--- Шаг %1 ------------------------------------------------")
        .arg(m_mw->m_currentStepIdx+1));
    m_mw->log(resolved.systemPrompt);
    if (!resolved.userPrompt.isEmpty())
        m_mw->log(QString("USER_QUERY:\n%1").arg(resolved.userPrompt));

    m_mw->m_streamBuf.clear();

    qDebug() << "[DEBUG SEND TO LLM] - USER PROMPT:" << resolved.userPrompt;
    qDebug() << "[DEBUG SEND TO LLM] - SYSTEM PROMPT (first 500 chars):" << resolved.systemPrompt.left(500);
    qDebug() << "[DEBUG SEND TO LLM] - SYSTEM PROMPT LENGTH:" << resolved.systemPrompt.length();

    m_mw->m_currentPrompt = resolved.systemPrompt + "\n" + resolved.userPrompt;

    if (m_mw->m_useAgentPipeline && ensureAgentModelConfigured()) {
        QVariantMap compileOpts;
        compileOpts.insert("temperature", 0.1);
        compileOpts.insert("num_predict", 1400);
        compileOpts.insert("timeoutMs", 90000);

        const QString compileSystem = resolved.systemPrompt + QStringLiteral("\n\nIMPORTANT: Output ONLY a JSON array of commands. No extra text.\n");
        const QString compiled = m_mw->m_agentModel.generateWithSystem(resolved.userPrompt, compileSystem, compileOpts);
        QTimer::singleShot(0, m_mw, [this, compiled]() {
            m_mw->m_streamBuf = compiled;
            handleAssistantFinished();
        });
        return;
    }

    if (!m_mw->m_activeImagesBase64.isEmpty()) {
        m_mw->m_model.startStreamWithSystemAndImages(resolved.userPrompt, resolved.systemPrompt, m_mw->m_activeImagesBase64, QVariantMap());
    } else {
        m_mw->m_model.startStreamWithSystem(resolved.userPrompt, resolved.systemPrompt);
    }
}

void SequenceRunner::addSequenceTransitionToHistory(const QString& sequenceName)
{
    QString transitionMsg = QString("Переход к последовательности: %1").arg(sequenceName);
    m_mw->m_history.push_back(qMakePair(QString("system"), transitionMsg));
}

void SequenceRunner::appendUserQuestionToResultDisplay(const QString& question)
{
    if (!m_mw->m_resultDisplayWidget)
        return;

    const QString trimmedQuestion = question.trimmed();
    if (trimmedQuestion.isEmpty())
        return;

    m_mw->m_lastQuestionTimestamp = QDateTime::currentDateTime();

    const QString formattedQuestion = QString("\n\n%1\n").arg(trimmedQuestion);
    m_mw->m_resultDisplayWidget->appendResult(m_mw->tr("Вопрос пользователя"),
                                        formattedQuestion,
                                        ResultDisplayWidget::EntryType::UserQuestion);
}

void SequenceRunner::stopExecution()
{
    m_mw->m_stopRequested = true;

    if (m_mw->m_sequenceMode) {
        m_mw->log("Шаблонное выполнение остановлено пользователем.");
        m_mw->m_statusController->stopTask();
    }
}
