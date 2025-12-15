#include "InputTextEdit.h"
#include <QBuffer>
#include <QDateTime>
#include <QFileInfo>
#include <QImage>
#include <QUrl>

InputTextEdit::InputTextEdit(QWidget *parent) : StyleTextEdit(parent)
{
    setAcceptDrops(true);

    connect(this, &InputTextEdit::textChanged, this, [this](){
        emit signal_textEdited(toPlainText());
    });
}

bool InputTextEdit::canInsertFromMimeData(const QMimeData *source) const
{
    if (!source)
        return false;

    if (source->hasImage())
        return true;

    if (source->hasUrls()) {
        const QList<QUrl> urls = source->urls();
        for (const QUrl &u : urls) {
            if (!u.isLocalFile())
                continue;
            const QString path = u.toLocalFile();
            const QString ext = QFileInfo(path).suffix().toLower();
            if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "bmp" || ext == "gif")
                return true;
        }
    }

    return StyleTextEdit::canInsertFromMimeData(source);
}

void InputTextEdit::insertFromMimeData(const QMimeData *source)
{
    if (!source) {
        StyleTextEdit::insertFromMimeData(source);
        return;
    }

    if (source->hasUrls()) {
        QStringList imageFiles;
        const QList<QUrl> urls = source->urls();
        for (const QUrl &u : urls) {
            if (!u.isLocalFile())
                continue;
            const QString path = u.toLocalFile();
            const QString ext = QFileInfo(path).suffix().toLower();
            if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "bmp" || ext == "gif") {
                imageFiles << path;
            }
        }
        if (!imageFiles.isEmpty()) {
            emit imagesInserted(imageFiles, QStringList(), QStringList());
            return;
        }
    }

    if (source->hasImage()) {
        const QVariant v = source->imageData();
        QImage img = qvariant_cast<QImage>(v);
        if (!img.isNull()) {
            QByteArray bytes;
            QBuffer buf(&bytes);
            buf.open(QIODevice::WriteOnly);
            img.save(&buf, "PNG");
            buf.close();

            const QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
            const QString name = QString("clipboard_%1.png").arg(ts);
            emit imagesInserted(QStringList(), QStringList(QString::fromLatin1(bytes.toBase64())), QStringList(name));
            return;
        }
    }

    StyleTextEdit::insertFromMimeData(source);
}
