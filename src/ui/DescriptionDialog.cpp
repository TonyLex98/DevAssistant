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
    DescriptionGenerator generator(m_model, m_devAss, this);
    QString descr = generator.generateAndSave(m_filePath);
    if (descr.isEmpty()) {
        QMessageBox::warning(this, "Генерация не удалась",
                            "Проверьте логи и подключение к модели.");
        return;
    }

    QMessageBox::information(this, "Готово",
                             "Описание сохранено.");
    accept();
}
