#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>

class ContextManager;
class MainWindow;

class ContextChainResolver : public QObject
{
    Q_OBJECT

public:
    explicit ContextChainResolver(ContextManager* contextManager, QObject *parent = nullptr);
    
    // Generate context from the command chain
    static QString resolveContextChain(MainWindow* mainWindow);

private:
    ContextManager* m_contextManager;
};