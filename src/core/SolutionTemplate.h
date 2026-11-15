#pragma once
#include <QString>
#include <QVector>
#include <QMap>
#include <QPointF>
#include <QJsonObject>
#include <QJsonArray>
#include <QPixmap>

class SolutionTemplate
{
public:
    SolutionTemplate() = default;
    explicit SolutionTemplate(const QString& name) : m_name(name) {}

    /*--- serialization ---------------------------------------------------*/
    QJsonObject toJson() const;
    static SolutionTemplate fromJson(const QJsonObject& obj);

    /*--- basic operations ------------------------------------------------*/
    void addSequence(const QString& sequencePath) { m_sequences.append(sequencePath); }
    void removeSequence(int idx) { m_sequences.removeAt(idx); }
    void removeSequenceByPath(const QString& sequencePath);
    void clearSequences() { m_sequences.clear(); }
    
    // Sequence target operations (for OPEN_SEQUENCE_INFO) - directed links
    void setSequenceTargets(const QJsonObject& targets) { m_sequenceTargets = targets; }
    QJsonObject getSequenceTargets() const { return m_sequenceTargets; }
    void setTargetsForSequence(const QString& seqName, const QJsonArray& targets);
    QJsonArray getTargetsForSequence(const QString& seqName) const;
    
    // Position operations
    void setPositionForSequence(const QString& sequenceName, const QPointF& position) { m_positions[sequenceName] = position; }
    QPointF getPositionForSequence(const QString& sequenceName) const { return m_positions.value(sequenceName, QPointF()); }
    void clearPositions() { m_positions.clear(); }
    const QMap<QString, QPointF>& positions() const { return m_positions; }

    /*--- getters ---------------------------------------------------------*/
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
    QVector<QString> m_sequences; // sequence file paths
    QJsonObject      m_sequenceTargets; // Directed links: sequence -> [target sequences] for OPEN_SEQUENCE_INFO
    QMap<QString, QPointF> m_positions; // sequence positions on the canvas
};