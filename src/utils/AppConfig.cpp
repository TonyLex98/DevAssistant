#include "AppConfig.h"
#include <QSettings>
#include <QCoreApplication>

void AppConfig::load(){
    QSettings s;
    lastProjectPath = s.value("ui/lastProjectPath").toString();
    selectedModel   = s.value("model/selected","").toString();
    endpoint        = QUrl(s.value("model/endpoint","http://127.0.0.1:11434").toString());
    params          = s.value("model/params").toMap();
}

void AppConfig::save() const{
    QSettings s;
    s.setValue("ui/lastProjectPath", lastProjectPath);
    s.setValue("model/selected", selectedModel);
    s.setValue("model/endpoint", endpoint.toString());
    s.setValue("model/params", params);
}
