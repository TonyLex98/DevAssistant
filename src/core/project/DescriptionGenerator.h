#include  <QString>
#include  "core/model/OllamaAdapter.h"
#include "core/project/DevAssManager.h"

/**
 * @brief Generates and saves file descriptions.
 *
 * Uses:
 *   - `OllamaAdapter::generate()` to produce the text
 *   - `DevAssManager` to compute the relative path, create
 *     `DevAss/<rel>.dadescr` and update .dapro
 *
 * @note A single instance is used (created in `MainWindow` and reused
 *       in `DescriptionDialog`).
 */
class DescriptionGenerator : public QObject
{
    Q_OBJECT
public:
    explicit DescriptionGenerator(OllamaAdapter* model, DevAssManager* devAss, QObject* parent = nullptr);

    /**
     * @brief Generates a file description and saves it to .dadescr.
     * @param filePath Absolute path to the source file.
     * @return The generated text, or an empty string on error.
     * @note If the description is already up to date, does nothing
     *       and returns an empty string.
     */
    QString generateAndSave(const QString& filePath);

private:
    QString createPrompt(const QString& content);
    QString writeDescription(const QString& relPath, const QString& description);
    QString relativePath(const QString& absPath);

    OllamaAdapter* m_model;
    DevAssManager* m_devAss;
};
