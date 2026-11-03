#include "FileClipboard.h"

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QUrl>

namespace colibry::fm {

namespace {
const QString kCutMime = QStringLiteral("application/x-colibry-cut");
}

void FileClipboard::set(const QStringList &paths, bool cut)
{
    auto *mime = new QMimeData;
    QList<QUrl> urls;
    for (const QString &path : paths)
        urls.append(QUrl::fromLocalFile(path));
    mime->setUrls(urls);
    mime->setData(kCutMime, cut ? QByteArrayLiteral("1") : QByteArrayLiteral("0"));
    QGuiApplication::clipboard()->setMimeData(mime);
}

ClipboardContent FileClipboard::get()
{
    ClipboardContent content;
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    if (!mime || !mime->hasUrls())
        return content;

    for (const QUrl &url : mime->urls()) {
        if (url.isLocalFile() && QFileInfo::exists(url.toLocalFile()))
            content.paths.append(url.toLocalFile());
    }
    content.cut = mime->data(kCutMime) == "1";
    return content;
}

bool FileClipboard::hasFiles()
{
    return !get().paths.isEmpty();
}

void FileClipboard::clear()
{
    QGuiApplication::clipboard()->clear();
}

} // namespace colibry::fm
