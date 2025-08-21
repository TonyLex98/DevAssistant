#include "WavWriter.h"
#include <QtEndian>

static void put32(char* dst, quint32 v) {
    dst[0] = char(v & 0xFF);
    dst[1] = char((v >> 8) & 0xFF);
    dst[2] = char((v >> 16) & 0xFF);
    dst[3] = char((v >> 24) & 0xFF);
}
static void put16(char* dst, quint16 v) {
    dst[0] = char(v & 0xFF);
    dst[1] = char((v >> 8) & 0xFF);
}

bool WavWriter::open(const QString& path, int sr, int ch, int bits) {
    m_path = path;
    m_sampleRate = sr; m_channels = ch; m_bits = bits;
    m_dataBytes = 0;
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly)) return false;
    writeHeader();
    m_open = true;
    return true;
}

void WavWriter::write(const char* data, int len) {
    if (!m_open) return;
    m_file.write(data, len);
    m_dataBytes += len;
}

void WavWriter::close() {
    if (!m_open) return;
    finalizeHeader();
    m_file.close();
    m_open = false;
}

void WavWriter::writeHeader() {
    // 44-байтный WAV PCM
    char hdr[44] = {};
    memcpy(hdr+0, "RIFF", 4);
    put32(hdr+4, 0); // заполнится в finalize
    memcpy(hdr+8, "WAVE", 4);
    memcpy(hdr+12, "fmt ", 4);
    put32(hdr+16, 16);                 // PCM chunk size
    put16(hdr+20, 1);                  // PCM
    put16(hdr+22, quint16(m_channels));
    put32(hdr+24, quint32(m_sampleRate));
    quint32 byteRate = quint32(m_sampleRate * m_channels * (m_bits/8));
    put32(hdr+28, byteRate);
    quint16 blockAlign = quint16(m_channels * (m_bits/8));
    put16(hdr+32, blockAlign);
    put16(hdr+34, quint16(m_bits));
    memcpy(hdr+36, "data", 4);
    put32(hdr+40, 0);                  // data size (позже)
    m_file.write(hdr, 44);
}

void WavWriter::finalizeHeader() {
    if (!m_file.isOpen()) return;
    // RIFF size = 36 + dataBytes
    quint32 riffSize = 36u + quint32(m_dataBytes);
    quint32 dataSize = quint32(m_dataBytes);
    m_file.seek(4);
    char tmp[4];
    put32(tmp, riffSize); m_file.write(tmp, 4);
    m_file.seek(40);
    put32(tmp, dataSize); m_file.write(tmp, 4);
}
