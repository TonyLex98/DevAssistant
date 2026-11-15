#pragma once
#include <QString>
#include <QStringList>

class QFileInfo;

namespace Rag {

// Cuts text into chunks of about `chunkChars` characters that overlap by about
// `overlapChars`, so a sentence on a boundary appears whole in at least one chunk. Breaks
// are placed at a paragraph, line, sentence or word boundary when one is near.
QStringList chunkText(const QString& text, int chunkChars = 1200, int overlapChars = 200);

// Whether a file is indexed: text, Markdown, source code, config and data files, and PDF.
bool isIndexable(const QFileInfo& file);

// The file's text. PDF goes through the pdftotext tool from Poppler, which has to be on PATH;
// everything else is read as UTF-8. Returns an empty string and sets `error` on failure.
QString extractText(const QString& path, QString* error);

} // namespace Rag
