#include "DescriptionDialog.h"
#include <QVBoxLayout>
#include <QMessageBox>
#include <QProgressDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QDebug>          // <‑‑ для отладки

DescriptionDialog::DescriptionDialog(const QString& filePath,
                                    OllamaAdapter* model,
                                    DevAssManager* devAss,
                                    QWidget* parent)
    : QDialog(parent), m_filePath(filePath),
      m_model(model), m_devAss(devAss)
{
    setWindowTitle("Описание файла");
    resize(420, 170);

    auto v = new QVBoxLayout(this);
    m_infoLabel = new QLabel(filePath, this);
    v->addWidget(m_infoLabel);

    m_genBtn = new QPushButton("Сгенерировать описание", this);
    v->addWidget(m_genBtn);

    connect(m_genBtn, &QPushButton::clicked,
            this, &DescriptionDialog::onGenerate);
}

void DescriptionDialog::onGenerate()
{
    /* ---------------------------------------------------------------- */
    /* 1. Читаем содержимое выбранного файла                            */
    /* ---------------------------------------------------------------- */
    QFile f(m_filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка чтения",
                             "Не удалось открыть файл:\n" + m_filePath);
        return;
    }
    QString content = QString::fromUtf8(f.readAll());
    f.close();

    /* ---------------------------------------------------------------- */
    /* 2. Формируем русский запрос к модели                              */
    /* ---------------------------------------------------------------- */
    const QString prompt = QString(
        "Пожалуйста, дайте краткое описание следующего содержимого файла.\n"
        "Если содержит код, перечислите основные классы/функции.\n"
        "Если это не код, дайте краткое резюме.\n\n%1").arg(content);

    /* ---------------------------------------------------------------- */
    /* 3. Отправляем запрос в Ollama (с простым прогресс‑диалогом)      */
    /* ---------------------------------------------------------------- */
    QProgressDialog prog("Генерация описания…", "Отмена",
                         0, 0, this);
    prog.setWindowModality(Qt::WindowModal);
    prog.show();

    const QString description = m_model->generate(prompt);

    if (description.isEmpty()) {
        QMessageBox::warning(this, "Ответ пустой",
                             "Получено пустое описание.");
        return;
    }

    /* ---------------------------------------------------------------- */
    /* 4. Путь к файлу‑дескриптору *.dadescr (внутри DevAss)             */
    /* ---------------------------------------------------------------- */
    // Относительный путь к файлу относительно **корня проекта**
    const QString relPath = QDir(m_devAss->projectRoot().absolutePath())
                            .relativeFilePath(m_filePath);

    // Путь к дескриптору внутри каталога DevAss
    const QString descrPath = m_devAss->devAssDir()
                                .filePath(relPath + ".dadescr");

    // ... после записи дескриптора
    // 5. Обновляем информацию в .dapro
    QDateTime now = QDateTime::currentDateTime();
    m_devAss->updateFileDescription(m_filePath, now, true);
    m_devAss->save();

    /* ---------------------------------------------------------------- */
    /* 5. Выводим отладочную информацию                                   */
    /* ---------------------------------------------------------------- */
    qDebug() << "Файл:"            << m_filePath;
    qDebug() << "Корень проекта:"  << m_devAss->projectRoot().absolutePath();
    qDebug() << "Относительный путь:" << relPath;
    qDebug() << "Путь дескриптора:"   << descrPath;

    /* ---------------------------------------------------------------- */
    /* 6. Создаём необходимые подкаталоги и сохраняем описание        */
    /* ---------------------------------------------------------------- */
    QDir().mkpath(QFileInfo(descrPath).absolutePath());  // создаём вложенные папки

    QFile df(descrPath);
    if (!df.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка записи",
                             "Не удалось записать файл описания:\n" + descrPath);
        return;
    }
    df.write(description.toUtf8());
    df.close();

    /* ---------------------------------------------------------------- */
    /* 7. Сообщаем пользователю и закрываем диалог                     */
    /* ---------------------------------------------------------------- */
    QMessageBox::information(this, "Готово",
                             "Описание сохранено в:\n" + descrPath);
    accept();
}
