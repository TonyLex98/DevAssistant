#include  <QString>
#include  <OllamaAdapter.h>
#include <DevAssManager.h>

/**
 * @brief Генерация и сохранение описания файла.
 *
 * Внутри использует:
 *   - `OllamaAdapter::generate()` для получения текста
 *   - `DevAssManager` для вычисления относительного пути, создания
 *     каталога `DevAss/<rel>.dadescr` и обновления .dapro
 *
 * API:
 *   - `generateAndSave()` – принимает абсолютный путь к исходному файлу,
 *     генерирует описание, сохраняет его в *.dadescr и обновляет .dapro.
 *
 * Возвращает: QString – сгенерированное описание (пустая строка – ошибка).
 *
 * @note Вызов `generateAndSave()` идёт только через один экземпляр
 *       класса (создаётся в `MainWindow` и переиспользуется в `DescriptionDialog`).
 */
class DescriptionGenerator : public QObject
{
    Q_OBJECT
public:
    explicit DescriptionGenerator(OllamaAdapter* model, DevAssManager* devAss, QObject* parent = nullptr);

    /**
     * @brief Генерирует описание файла и сохраняет его в .dadescr.
     * @param filePath Абсолютный путь к исходному файлу.
     * @return Сгенерированный текст. В случае ошибки – пустая строка.
     * @note Если описание уже актуально, метод ничего не делает
     *       и возвращает пустую строку (в клиенте можно отобразить «уже актуально»).
     */
    QString generateAndSave(const QString& filePath);

private:
    QString createPrompt(const QString& content);
    QString writeDescription(const QString& relPath, const QString& description);
    QString relativePath(const QString& absPath);

    OllamaAdapter* m_model;
    DevAssManager* m_devAss;
};
