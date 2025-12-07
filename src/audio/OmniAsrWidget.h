#pragma once
#include <QWidget>
#include "../ui/customWidgets/StyledButton.h"
#include "OmniAsrRunner.h"

class OmniAsrWidget : public QWidget {
    Q_OBJECT
public:
    explicit OmniAsrWidget(QWidget* parent = nullptr);

signals:
    void textRecognized(const QString& text);

private slots:
    void onBtnClicked();
    void onRecognitionFinished(const QString& text);
    void onError(const QString& msg);

private:
    StyledButton* m_recordBtn;
    OmniAsrRunner* m_runner;
    bool m_isRecording;
};
