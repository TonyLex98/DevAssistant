#pragma once
#include <QString>
#include <QVariantMap>
#include <QUrl>

class AppConfig {
public:
    QString lastProjectPath;
    QString selectedModel;
    QUrl    endpoint;
    QVariantMap params;

    void load();
    void save() const;
};
