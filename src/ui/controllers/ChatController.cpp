#include "ChatController.h"
#include "../../utils/json_utils.h"
#include <QDebug>

ChatController::ChatController(IContextService* ctx, IModelService* model, ISequenceService* seq, IUIUpdater* ui)
    : QObject(nullptr)
    , m_ctx(ctx)
    , m_model(model)
    , m_seq(seq)
    , m_ui(ui)
{
}

void ChatController::sendUserMessage(const QString& text)
{
    m_ctx->observeUserQuery(text);

    m_model->setLastUserMessage(text);
    qDebug() << "[sendUserMessage] updated last user message =" << m_model->lastUserMessage();
    m_ui->appendUserQuestionToResultDisplay(text);

    // Add to main history
    m_model->appendHistory("user", text);
    // Also add to separate user messages storage (for LAST_USER_MESSAGES resolver)
    m_model->appendLastUserMessage(text);

    m_seq->resetSequenceStabilityState();

    m_seq->runSequenceDialog();

    qDebug() << "[sendUserMessage] runSequenceDialog completed";
}

void ChatController::onModelChunk(const QString& s)
{
    m_model->streamBuf() += s;
    m_model->appendAssistantChunk(s);
}

QByteArray ChatController::extractJsonPart(const QByteArray& txt) const
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
