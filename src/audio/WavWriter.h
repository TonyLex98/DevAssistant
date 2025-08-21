#pragma once
#include <QFile>

class WavWriter {
public:
    bool open(const QString& path, int sampleRate = 16000, int channels = 1, int bitsPerSample = 16);
    void write(const char* data, int len);
    void close();
    QString path() const { return m_path; }
private:
    void writeHeader();
    void finalizeHeader();

    QFile m_file;
    QString m_path;
    int m_sampleRate = 16000;
    int m_channels = 1;
    int m_bits = 16;
    qint64 m_dataBytes = 0;
    bool m_open = false;
};
