#pragma once

/// @file Fragment.h
/// @brief Data structure representing a text fragment and its embedding.

namespace Rag
{
struct Fragment
{
    QString filePath;
    int pageNumber = -1;
    int paragraphNumber = -1;
    QVector<float> embedding; // 768‑dim vector
    QString text;
};

} // namespace Rag

