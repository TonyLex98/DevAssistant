#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include "core/model/ModelReply.h"

// What to ask a model.
struct ModelRequest
{
    QString system;            // instructions; may be empty
    QString user;              // the message
    QStringList imagesBase64;  // attached images, for models that read them
    QVariantMap options;       // generation options for this request only (temperature, num_predict...)
    bool stream = false;       // deliver the answer piece by piece through ModelReply::chunk()
    int timeoutMs = 120000;    // the whole request; 0 means no limit
};

// A language model the program can talk to. send() returns at once; the answer arrives
// through the returned reply. LlmClient talks to real servers; tests use their own clients.
class ModelClient : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    ~ModelClient() override = default;

    // The model's name; empty when none is selected.
    virtual QString model() const = 0;
    virtual ModelReply* send(const ModelRequest& request) = 0;
    // A rough size of a text in tokens.
    virtual int estimateTokens(const QString& text) const { return text.size() / 4 + 1; }
};
