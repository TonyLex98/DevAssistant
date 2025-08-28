// DescriptionGenerator.cpp
#include "DescriptionGenerator.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>

DescriptionGenerator::DescriptionGenerator(OllamaAdapter* model,
                                         DevAssManager* devAss,
                                         QObject* parent)
    : QObject(parent), m_model(model), m_devAss(devAss)
{
}

QString DescriptionGenerator::relativePath(const QString& absPath)
{
    return QDir(m_devAss->projectRoot().absolutePath())
                .relativeFilePath(absPath);
}

QString DescriptionGenerator::createPrompt(const QString& content)
{
    return QString(
        "Составь формализованное, но очень краткое описание содержимого файла. Пиши по-русски и строго следуй формату ниже.\n"
        "\n"
        "Если файл содержит код, выполни статический разбор и выведи в Markdown:\n"
        "## Назначение файла\n"
        "- 1–2 предложения о том, что делает файл.\n"
        "\n"
        "## Сущности\n"
        "Таблица (Имя | Тип {class/struct/enum/namespace} | Назначение в 1 предложении).\n"
        "\n"
        "## Методы по классам\n"
        "Для каждого класса перечисли методы строками вида:\n"
        "- Class::method(params) — краткое назначение; вызывает: A::x, B::y; изменяет: поля; бросает: исключения; сигналы/слоты: …\n"
        "\n"
        "## Взаимодействия\n"
        "Список связей «источник → цель» с пояснением (когда/зачем), например:\n"
        "- A::foo → B::bar — при обработке события …\n"
        "Если явных вызовов нет — так и укажи.\n"
        "\n"
        "## Зависимости\n"
        "Внешние библиотеки/Qt-классы и для чего они используются.\n"
        "\n"
        "## Публичный API\n"
        "Ключевые точки входа (короткий перечень).\n"
        "\n"
        "Если это НЕ код, дай сжатое резюме в 2–3 предложениях.\n"
        "Не выдумывай фактов; опирайся только на текст ниже.\n"
        "\n"
        "-----\n"
        "%1"
    ).arg(content);
}


QString DescriptionGenerator::writeDescription(const QString& relPath,
                                              const QString& description)
{
    // Путь к *.dadescr в каталоге DevAss
    QString descrPath = m_devAss->devAssDir().filePath(relPath + ".dadescr");

    // Создаём вложенные каталоги, если нужно
    QDir().mkpath(QFileInfo(descrPath).absolutePath());

    QFile f(descrPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Не удалось записать дескриптор:" << descrPath;
        return QString();
    }
    f.write(description.toUtf8());
    f.close();

    return descrPath;
}

QString DescriptionGenerator::generateAndSave(const QString& filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile())
        return QString();

    // --- 1. Проверяем актуальность ---
    FileDescription* fd = m_devAss->findFileDescription(filePath);
    QDateTime fileMod = fi.lastModified();
    if (fd && fd->isDescriptionUpToDate && fileMod <= fd->lastModTime)
        return QString(); // уже актуально – ничего не делаем

    // --- 2. Читаем файл ---
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Не удалось открыть файл:" << filePath;
        return QString();
    }
    QString content = QString::fromUtf8(f.readAll());
    f.close();

    // --- 3. Генерируем через модель ---
    QString prompt = createPrompt(content);
    QString description = m_model->generate(prompt);
    if (description.isEmpty()) {
        qWarning() << "LLM вернул пустое описание для:" << filePath;
        return QString();
    }

    // --- 4. Записываем в *.dadescr ---
    QString relPath = relativePath(filePath);
    QString descrPath = writeDescription(relPath, description);
    if (descrPath.isEmpty())
        return QString(); // ошибка записи

    // --- 5. Обновляем .dapro ---
    m_devAss->updateFileDescription(filePath,
                                    QDateTime::currentDateTime(),
                                    true);
    m_devAss->save();

    return description;
}
