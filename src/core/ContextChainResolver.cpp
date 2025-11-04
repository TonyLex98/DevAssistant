#include "ContextChainResolver.h"
#include "ContextManager.h"
#include "../ui/MainWindow.h"

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
    
    // Get context based on valid commands from the command chain
    // This will be implemented in MainWindow to access the private ContextManager
    return mainWindow->getContextChainText();
}