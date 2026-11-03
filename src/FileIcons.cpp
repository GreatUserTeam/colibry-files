#include "FileIcons.h"

#include <QApplication>
#include <QFileInfo>
#include <QStyle>

namespace colibry::fm {

FileIcons *FileIcons::shared()
{
    static FileIcons *instance = new FileIcons;
    return instance;
}

QIcon FileIcons::fallback(bool isDir, bool isLink)
{
    const QStyle *style = QApplication::style();
    if (isLink)
        return style->standardIcon(isDir ? QStyle::SP_DirLinkIcon : QStyle::SP_FileLinkIcon);
    return style->standardIcon(isDir ? QStyle::SP_DirIcon : QStyle::SP_FileIcon);
}

QIcon FileIcons::icon(IconType type) const
{
    const QIcon base = QFileIconProvider::icon(type);
    if (!base.isNull())
        return base;

    const QStyle *style = QApplication::style();
    switch (type) {
    case Folder:
        return style->standardIcon(QStyle::SP_DirIcon);
    case Drive:
        return style->standardIcon(QStyle::SP_DriveHDIcon);
    case Network:
        return style->standardIcon(QStyle::SP_DriveNetIcon);
    case Computer:
        return style->standardIcon(QStyle::SP_ComputerIcon);
    case Desktop:
        return style->standardIcon(QStyle::SP_DesktopIcon);
    case Trashcan:
        return style->standardIcon(QStyle::SP_TrashIcon);
    case File:
    default:
        return style->standardIcon(QStyle::SP_FileIcon);
    }
}

QIcon FileIcons::icon(const QFileInfo &info) const
{
    const QIcon base = QFileIconProvider::icon(info);
    if (!base.isNull())
        return base;
    return fallback(info.isDir(), info.isSymLink());
}

} // namespace colibry::fm
