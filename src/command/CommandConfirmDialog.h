// CommandConfirmDialog.h
#pragma once
#include <QDialog>
#include <QList>
#include "Command.h"

class CommandConfirmDialog : public QDialog
{
    Q_OBJECT
public:
    CommandConfirmDialog(const QList<Command*>& cmds,
                        QWidget* parent = nullptr,
                        bool defaultChecked = false); // Added parameter to set default checked state

    CommandConfirmDialog(){

    };
    // Возвращает список тех команд, которые пользователь подтвердили
    QList<Command*> confirmed() const { return m_confirmed; }

private slots:
    void onAccept();
    void onReject();

private:
    QList<Command*> m_commands;
    QList<Command*> m_confirmed;
};
