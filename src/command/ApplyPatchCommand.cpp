// ApplyPatchCommand.cpp
#include "ApplyPatchCommand.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include "../utils/TextFileLines.h"

struct PatchHunkLine {
    QChar kind; // ' ', '+', '-', '\\'
    QString text;
};

struct PatchHunk {
    int oldStart = 0;
    int oldCount = 0;
    int newStart = 0;
    int newCount = 0;
    QList<PatchHunkLine> lines;
};

struct FilePatch {
    QString oldPath;
    QString newPath;
    bool isCreate = false;
    bool isDelete = false;
    QList<PatchHunk> hunks;
};

static QString stripPathComponents(QString p, int strip)
{
    p = p.trimmed();
    if (p == "/dev/null")
        return p;
    p = p.replace('\\', '/');
    while (strip > 0) {
        int idx = p.indexOf('/');
        if (idx < 0)
            break;
        p = p.mid(idx + 1);
        strip--;
    }
    return p;
}

// The path from a "--- " / "+++ " header: anything after a tab (diff -u puts a timestamp
// there) is dropped; with strip == 0 git's a/ and b/ prefixes are removed, otherwise `strip`
// leading components are, the prefix counting as one of them.
static QString headerPath(const QString& headerLine, int strip)
{
    QString p = headerLine.mid(4);
    const int tab = p.indexOf(QLatin1Char('\t'));
    if (tab >= 0)
        p = p.left(tab);
    p = p.trimmed();
    if (p == QLatin1String("/dev/null"))
        return p;
    if (strip == 0) {
        if (p.startsWith(QLatin1String("a/")) || p.startsWith(QLatin1String("b/")))
            p = p.mid(2);
        return p;
    }
    return stripPathComponents(p, strip);
}

static bool applyHunks(QStringList& lines, const QList<PatchHunk>& hunks, QString* err)
{
    int delta = 0;
    for (const PatchHunk& h : hunks) {
        if (h.oldStart <= 0) {
            if (err) *err = "APPLY_PATCH: invalid hunk header";
            return false;
        }
        int cursor = (h.oldStart - 1) + delta;
        if (cursor < 0) cursor = 0;
        if (cursor > lines.size()) cursor = lines.size();

        for (const PatchHunkLine& hl : h.lines) {
            if (hl.kind == '\\') {
                // "\ No newline at end of file" marker; ignore.
                continue;
            }
            if (hl.kind == ' ') {
                if (cursor >= lines.size() || lines.at(cursor) != hl.text) {
                    if (err) *err = QString("APPLY_PATCH: context mismatch at line %1").arg(cursor + 1);
                    return false;
                }
                cursor++;
                continue;
            }
            if (hl.kind == '-') {
                if (cursor >= lines.size() || lines.at(cursor) != hl.text) {
                    if (err) *err = QString("APPLY_PATCH: delete mismatch at line %1").arg(cursor + 1);
                    return false;
                }
                lines.removeAt(cursor);
                delta -= 1;
                continue;
            }
            if (hl.kind == '+') {
                lines.insert(cursor, hl.text);
                cursor++;
                delta += 1;
                continue;
            }
            if (err) *err = "APPLY_PATCH: unknown patch line type";
            return false;
        }
    }
    return true;
}

static bool parseUnifiedDiff(const QString& text, int strip, QList<FilePatch>& out, QString* err)
{
    QStringList lines = text.split('\n', Qt::KeepEmptyParts);
    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();

    FilePatch current;
    bool hasFile = false;
    PatchHunk currentHunk;
    bool inHunk = false;

    auto flushHunk = [&]() {
        if (inHunk) {
            current.hunks.append(currentHunk);
            currentHunk = PatchHunk();
            inHunk = false;
        }
    };
    auto flushFile = [&]() {
        flushHunk();
        if (hasFile) {
            out.append(current);
            current = FilePatch();
            hasFile = false;
        }
    };

    QRegularExpression hunkRe(R"(^@@\s*-(\d+)(?:,(\d+))?\s+\+(\d+)(?:,(\d+))?\s*@@)");

    // Lines still expected by the current hunk, from its header counts. While a hunk is
    // not complete every line belongs to it, so a removed line "-- x" (shown as "--- x")
    // is not mistaken for the next file's header.
    int oldLeft = 0;
    int newLeft = 0;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);
        if (!line.isEmpty() && line.endsWith('\r'))
            line.chop(1);

        if (inHunk && (oldLeft > 0 || newLeft > 0)) {
            const QChar kind = line.isEmpty() ? QChar(' ') : line.at(0);
            if (kind == '\\') {
                currentHunk.lines.append(PatchHunkLine{kind, line});
                continue;
            }
            if (kind == ' ' || kind == '+' || kind == '-') {
                currentHunk.lines.append(PatchHunkLine{kind, line.isEmpty() ? QString() : line.mid(1)});
                if (kind != '+') --oldLeft;
                if (kind != '-') --newLeft;
                continue;
            }
            // Anything else ends a hunk whose header undercounted.
        }
        if (inHunk && !line.isEmpty() && line.at(0) == '\\') {
            currentHunk.lines.append(PatchHunkLine{line.at(0), line});
            continue;
        }

        if (line.startsWith("--- ")) {
            flushFile();
            hasFile = true;
            current.oldPath = headerPath(line, strip);
            continue;
        }
        if (hasFile && line.startsWith("+++ ")) {
            current.newPath = headerPath(line, strip);
            current.isCreate = (current.oldPath == "/dev/null");
            current.isDelete = (current.newPath == "/dev/null");
            continue;
        }

        const auto m = hunkRe.match(line);
        if (hasFile && m.hasMatch()) {
            flushHunk();
            inHunk = true;
            currentHunk.oldStart = m.captured(1).toInt();
            currentHunk.oldCount = m.captured(2).isEmpty() ? 1 : m.captured(2).toInt();
            currentHunk.newStart = m.captured(3).toInt();
            currentHunk.newCount = m.captured(4).isEmpty() ? 1 : m.captured(4).toInt();
            oldLeft = currentHunk.oldCount;
            newLeft = currentHunk.newCount;
            continue;
        }
    }

    flushFile();
    if (out.isEmpty()) {
        if (err) *err = "APPLY_PATCH: no file diffs found";
        return false;
    }
    return true;
}

ApplyPatchCommand::ApplyPatchCommand(const QByteArray& patch, int strip, const QString& descr)
{
    m_params["patch"] = QString::fromUtf8(patch);
    m_params["strip"] = strip;
    m_description = descr.isEmpty()
        ? QStringLiteral("Applies a unified diff (like git diff) to the project. "
                         "Example: { \"command\": \"APPLY_PATCH\", \"params\": { \"patch\": \"diff --git a/a.txt b/a.txt\\n...\", \"strip\": 0 } }")
        : descr;
}

static QString normalizePathMaybe(DevAssManager* devAss, const QString& rel)
{
    if (!devAss)
        return rel;
    if (QFileInfo(rel).isAbsolute())
        return QDir::cleanPath(rel);
    return QDir(devAss->projectRoot().absolutePath()).filePath(rel);
}

bool ApplyPatchCommand::apply(DevAssManager* devAss, QString* err)
{
    m_summary.clear();
    if (!devAss) {
        if (err) *err = "APPLY_PATCH: DevAssManager is required";
        return false;
    }

    const QString patchText = m_params.value("patch").toString();
    const int strip = m_params.value("strip").toInt(0);
    if (patchText.trimmed().isEmpty()) {
        if (err) *err = "APPLY_PATCH: patch is empty";
        return false;
    }

    QList<FilePatch> patches;
    if (!parseUnifiedDiff(patchText, strip, patches, err))
        return false;

    QList<Change> changes;
    QStringList applied;

    for (const FilePatch& fp : patches) {
        const QString relTarget = fp.isDelete ? fp.oldPath : fp.newPath;
        if (relTarget == "/dev/null") {
            if (err) *err = "APPLY_PATCH: invalid /dev/null target";
            return false;
        }
        const QString absTarget = normalizePathMaybe(devAss, relTarget);

        if (fp.isCreate) {
            QStringList newLines;
            // Build created content from hunks (+ and ' ' lines that are new file lines).
            for (const PatchHunk& h : fp.hunks) {
                for (const PatchHunkLine& hl : h.lines) {
                    if (hl.kind == '+' || hl.kind == ' ')
                        newLines << hl.text;
                }
            }
            TextFileLines created;
            created.lines = newLines;
            changes << Change::createFile(absTarget, created.toBytes());
            applied << QString("CREATE %1").arg(relTarget);
            continue;
        }

        if (fp.isDelete) {
            changes << Change::deleteFile(absTarget);
            applied << QString("DELETE %1").arg(fp.oldPath);
            continue;
        }

        // Modify existing file.
        if (!QFileInfo::exists(absTarget)) {
            if (err) *err = QString("APPLY_PATCH: file not found: %1").arg(relTarget);
            return false;
        }

        TextFileLines file;
        if (!TextFileLines::load(absTarget, &file, err))
            return false;
        QStringList& fileLines = file.lines;

        if (!applyHunks(fileLines, fp.hunks, err)) {
            if (err && !fp.newPath.trimmed().isEmpty())
                *err = QString("%1 (file: %2)").arg(*err, fp.newPath);
            return false;
        }

        changes << Change::replaceInFile(absTarget, file.toBytes());
        applied << QString("PATCH %1").arg(relTarget);
    }

    if (!devAss->applyChanges(changes, err))
        return false;

    m_summary = applied.join("\n");
    return true;
}

QJsonObject ApplyPatchCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString ApplyPatchCommand::getResult() const
{
    const bool ok = wasSuccessful();
    QString result = QString("APPLY_PATCH\n[status]\n%1\n").arg(ok ? "Patch applied successfully" : "Patch apply failed");
    if (!m_summary.trimmed().isEmpty())
        result += QString("[applied]\n%1\n").arg(m_summary.trimmed());
    result += QString("exitCode=%1").arg(ok ? 0 : 1);
    if (!getErrorMessage().isEmpty())
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    return result;
}
