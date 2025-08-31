// DescriptionDialog.h
#pragma once

#include <QDialog>
#include <QLabel>
#include <StyledButton.h>
#include "../adapters/OllamaAdapter.h"
#include "../core/DevAssManager.h"
#include <DescriptionGenerator.h>


class DescriptionDialog : public QDialog
{
    Q_OBJECT
public:
    // filePath – абсолютный путь к исходному файлу
    DescriptionDialog(const QString& filePath,
                      OllamaAdapter* model,
                      DevAssManager* devAss,
                      QWidget* parent = nullptr);

private slots:
    void onGenerate();   // «Сгенерировать описание»

private:
    QString          m_filePath;
    OllamaAdapter*   m_model;
    DevAssManager*   m_devAss;
    QLabel*          m_infoLabel = nullptr;
    StyledButton*     m_genBtn = nullptr;

};
