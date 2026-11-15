#include "core/rag/RagText.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>

namespace Rag {

namespace {

// The best place to end a chunk inside [from, to): the last paragraph break, else line
// break, else sentence end, else space. Returns `to` when there is none.
int breakPoint(const QString& text, int from, int to)
{
    const QStringList separators = {QStringLiteral("\n\n"), QStringLiteral("\n"),
                                    QStringLiteral(". "), QStringLiteral(" ")};
    for (const QString& sep : separators) {
        const int at = text.lastIndexOf(sep, to - sep.size());
        if (at >= from)
            return at + sep.size();
    }
    return to;
}

} // namespace

QStringList chunkText(const QString& text, int chunkChars, int overlapChars)
{
    chunkChars = qMax(100, chunkChars);
    overlapChars = qBound(0, overlapChars, chunkChars / 2);

    QString t = text;
    t.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    t.replace(QRegularExpression(QStringLiteral("\n{3,}")), QStringLiteral("\n\n"));
    t = t.trimmed();

    QStringList chunks;
    if (t.isEmpty())
        return chunks;
    if (t.size() <= chunkChars)
        return {t};

    int pos = 0;
    const int n = t.size();
    while (pos < n) {
        int end = qMin(n, pos + chunkChars);
        if (end < n)
            end = breakPoint(t, pos + chunkChars / 2, end);
        const QString piece = t.mid(pos, end - pos).trimmed();
        if (!piece.isEmpty())
            chunks << piece;
        if (end >= n)
            break;
        // Step back for the overlap, then forward to the start of a word.
        int next = qMax(pos + 1, end - overlapChars);
        while (next < end && !t.at(next - 1).isSpace())
            ++next;
        pos = next;
    }
    return chunks;
}

bool isIndexable(const QFileInfo& file)
{
    static const QSet<QString> suffixes = {
        "txt", "md", "markdown", "rst", "adoc", "org", "tex", "csv", "tsv", "log",
        "json", "yaml", "yml", "toml", "ini", "cfg", "conf", "xml", "html", "htm",
        "c", "h", "cc", "cpp", "cxx", "hh", "hpp", "cs", "java", "kt", "go", "rs", "py", "js",
        "ts", "tsx", "jsx", "dart", "swift", "rb", "php", "sh", "ps1", "bat", "sql", "qml",
        "pro", "pri", "cmake", "gradle",
        "pdf",
    };
    const QString name = file.fileName().toLower();
    return suffixes.contains(file.suffix().toLower()) || name == QLatin1String("cmakelists.txt")
            || name == QLatin1String("readme") || name == QLatin1String("makefile");
}

QString extractText(const QString& path, QString* error)
{
    const QFileInfo fi(path);
    if (fi.suffix().compare(QLatin1String("pdf"), Qt::CaseInsensitive) == 0) {
        QProcess p;
        p.start(QStringLiteral("pdftotext"), {QStringLiteral("-enc"), QStringLiteral("UTF-8"), path, QStringLiteral("-")});
        if (!p.waitForStarted(5000)) {
            if (error)
                *error = QCoreApplication::translate("Rag", "PDF needs the pdftotext tool (Poppler) on PATH.");
            return QString();
        }
        if (!p.waitForFinished(120000) || p.exitCode() != 0) {
            if (error)
                *error = QCoreApplication::translate("Rag", "pdftotext could not read %1.").arg(fi.fileName());
            return QString();
        }
        return QString::fromUtf8(p.readAllStandardOutput());
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QCoreApplication::translate("Rag", "Cannot open %1.").arg(fi.fileName());
        return QString();
    }
    const QByteArray data = f.read(8 * 1024 * 1024); // larger files are cut at 8 MB
    if (data.contains('\0')) {
        if (error)
            *error = QCoreApplication::translate("Rag", "%1 is not a text file.").arg(fi.fileName());
        return QString();
    }
    return QString::fromUtf8(data);
}

} // namespace Rag
