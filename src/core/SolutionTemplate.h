#pragma once
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QPixmap>

class SolutionTemplate
{
public:
    SolutionTemplate() = default;
    explicit SolutionTemplate(const QString& name) : m_name(name) {}

    /*--- сериализация ---------------------------------------------------*/
    QJsonObject toJson() const;
    static SolutionTemplate fromJson(const QJsonObject& obj);

    /*--- базовые операции -----------------------------------------------*/
    void addSequence(const QString& sequencePath) { m_sequences.append(sequencePath); }
    void removeSequence(int idx) { m_sequences.removeAt(idx); }
    void removeSequenceByPath(const QString& sequencePath);
    void clearSequences() { m_sequences.clear(); }

    /*--- геттеры --------------------------------------------------------*/
    const QString&               name() const { return m_name; }
    const QString&               description() const { return m_description; }
    const QString&               imagePath() const { return m_imagePath; }
    const QString&               mainSequencePath() const { return m_mainSequencePath; }
    const QVector<QString>&      sequences() const { return m_sequences; }

    void setName(const QString& n) { m_name = n; }
    void setDescription(const QString& d) { m_description = d; }
    void setImagePath(const QString& path) { m_imagePath = path; }
    void setMainSequencePath(const QString& path) { m_mainSequencePath = path; }

private:
    QString          m_name;
    QString          m_description;
    QString          m_imagePath;
    QString          m_mainSequencePath;
    QVector<QString> m_sequences; // Пути к файлам последовательностей
};