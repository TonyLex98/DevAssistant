#pragma once
#include <QString>
#include <QVariantMap>

class ModelAdapter {
public:
    virtual ~ModelAdapter() {}
    virtual void configure(const QString& model, const QVariantMap& options) = 0;
    virtual QString generate(const QString& prompt, const QVariantMap& options = QVariantMap()) = 0;
    virtual int estimateTokens(const QString& text) const = 0;
    virtual bool isAvailable(QString* errorOut = nullptr) = 0;
};
