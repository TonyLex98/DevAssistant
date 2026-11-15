#pragma once
#include <QList>
#include <QString>
#include <QStringList>

// One message of the conversation with the model.
struct ChatMessage
{
    QString role;     // "user", "assistant" or "system"
    QString content;
};

// What the user asked and what the model and the program answered, as the prompts show it.
class Conversation
{
public:
    static constexpr int kMaxRecentRequests = 100;

    // The user's request. A request equal to the previous one is not repeated.
    void addUserMessage(const QString& text);
    // The user's answer to a question of the model: history only, the request stays the same.
    void addUserReply(const QString& text);
    void addAssistantMessage(const QString& text);
    // Something the program tells the model: command results, errors, sequence switches.
    void addNote(const QString& text);

    // The request being worked on (typed, dictated or sent last).
    QString currentRequest() const { return m_currentRequest; }
    void setCurrentRequest(const QString& text);
    QString lastAssistantMessage() const { return m_lastAssistantMessage; }

    const QList<ChatMessage>& history() const { return m_history; }
    // The last requests, oldest first.
    const QStringList& recentRequests() const { return m_recentRequests; }
    // The last `maxMessages` messages as "role: text" lines, each cut to `maxChars`.
    QString historyText(int maxMessages, int maxChars) const;

    // Images attached to the current request.
    struct Images
    {
        QStringList paths;
        QStringList base64;
        QStringList summaries;
        bool isEmpty() const { return paths.isEmpty() && base64.isEmpty(); }
    };
    const Images& images() const { return m_images; }
    void setImages(const Images& images) { m_images = images; }
    void clearImages() { m_images = Images(); }

private:
    QList<ChatMessage> m_history;
    QStringList m_recentRequests;
    QString m_currentRequest;
    QString m_lastAssistantMessage;
    Images m_images;
};
