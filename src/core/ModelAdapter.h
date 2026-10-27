#pragma once
#include <QString>
#include <QVariantMap>

class ModelAdapter {
public:
    virtual ~ModelAdapter() {}
    virtual void configure(const QString& model, const QVariantMap& options) = 0;
    virtual QString generate(const QString& prompt, const QVariantMap& options = QVariantMap()) = 0;

    /* Generate with separate system and user prompts.
       Default implementation combines them into a single prompt.
       Subclasses can override for native system/user support. */
    virtual QString generateWithSystem(const QString& userPrompt,
                                       const QString& systemPrompt,
                                       const QVariantMap& options = QVariantMap()) {
        // Default: combine system + user into single prompt
        QString combined = systemPrompt + "\n\n" + userPrompt;
        return generate(combined, options);
    }

    virtual int estimateTokens(const QString& text) const = 0;
    virtual bool isAvailable(QString* errorOut = nullptr) = 0;
    virtual QString selectedModel() const = 0;
};
