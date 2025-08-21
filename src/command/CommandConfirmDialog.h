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
                        QWidget* parent = nullptr);

    CommandConfirmDialog(){

    };
    // Возвращает список тех команд, которые пользователь подтвердил
    QList<Command*> confirmed() const { return m_confirmed; }

private slots:
    void onAccept();
    void onReject();

private:
    QList<Command*> m_commands;
    QList<Command*> m_confirmed;
};
