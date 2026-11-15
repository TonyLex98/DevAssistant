#include "core/context/ContextChainResolver.h"
#include "core/context/ContextManager.h"
#include "ui/MainWindow.h"

ContextChainResolver::ContextChainResolver(ContextManager* contextManager, QObject *parent)
    : QObject(parent)
    , m_contextManager(contextManager)
{
}

QString ContextChainResolver::resolveContextChain(MainWindow* mainWindow)
{
    if (!mainWindow) {
        return QString();
    }
    
    // Get full command chain - all commands that were applied, regardless of current validity
    // This shows the complete history of applied commands
    return mainWindow->getAllCommandsText();
}