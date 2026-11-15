#include "core/session/Conversation.h"

void Conversation::addUserMessage(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;
    m_currentRequest = text;
    if (!m_recentRequests.isEmpty() && m_recentRequests.last() == text)
        return;
    m_history.append({QStringLiteral("user"), text});
    m_recentRequests.append(text);
    while (m_recentRequests.size() > kMaxRecentRequests)
        m_recentRequests.removeFirst();
}

void Conversation::addUserReply(const QString& text)
{
    if (text.trimmed().isEmpty())
        return;
    m_history.append({QStringLiteral("user"), text});
    m_recentRequests.append(text);
    while (m_recentRequests.size() > kMaxRecentRequests)
        m_recentRequests.removeFirst();
}

void Conversation::addAssistantMessage(const QString& text)
{
    m_history.append({QStringLiteral("assistant"), text});
    m_lastAssistantMessage = text;
}

void Conversation::addNote(const QString& text)
{
    if (!text.trimmed().isEmpty())
        m_history.append({QStringLiteral("system"), text});
}

void Conversation::setCurrentRequest(const QString& text)
{
    if (!text.trimmed().isEmpty())
        m_currentRequest = text;
}

QString Conversation::historyText(int maxMessages, int maxChars) const
{
    QString out;
    for (int i = qMax(0, m_history.size() - maxMessages); i < m_history.size(); ++i) {
        QString msg = m_history.at(i).content;
        if (maxChars > 0 && msg.size() > maxChars)
            msg = msg.left(maxChars) + QStringLiteral("...");
        out += m_history.at(i).role + QStringLiteral(": ") + msg + QLatin1Char('\n');
    }
    return out.trimmed();
}
