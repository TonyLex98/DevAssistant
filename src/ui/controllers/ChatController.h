#pragma once
#include <QObject>
#include "../../services/IService.h"

class ChatController : public QObject {
    Q_OBJECT
public:
    ChatController(IContextService* ctx, IModelService* model, ISequenceService* seq, IUIUpdater* ui);

    void sendUserMessage(const QString& text);
    void onModelChunk(const QString& s);
    QByteArray extractJsonPart(const QByteArray& txt) const;

private:
    IContextService* m_ctx;
    IModelService* m_model;
    ISequenceService* m_seq;
    IUIUpdater* m_ui;
};
