#pragma once
#include <QString>
#include <QVector>

// Document search (retrieval-augmented generation) for DevAssistant.
//
// Files in a folder are cut into overlapping chunks, each chunk gets an embedding vector from
// the model server (Ollama or any OpenAI-compatible server), and the chunks are stored in
// SQLite next to the project. A question is embedded the same way and answered from the
// closest chunks, with the sources shown.
namespace Rag {

struct Chunk
{
    QString path;        // absolute file path
    int ordinal = 0;     // position of the chunk in its file, from 0
    QString text;
    QVector<float> embedding;
};

struct Hit
{
    Chunk chunk;
    float score = 0.0f;  // cosine similarity to the query, -1..1
};

} // namespace Rag
