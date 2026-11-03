#include "FileOperations.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QStorageInfo>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace colibry::fm {

namespace {

const QStringList &doubleSuffixes()
{
    static const QStringList s = {QStringLiteral(".tar.gz"), QStringLiteral(".tar.xz"),
                                  QStringLiteral(".tar.bz2"), QStringLiteral(".tar.zst")};
    return s;
}

/// Splits "photo.jpg" into ("photo", ".jpg") and "a.tar.gz" into ("a", ".tar.gz").
QPair<QString, QString> splitName(const QString &name)
{
    for (const QString &suffix : doubleSuffixes()) {
        if (name.endsWith(suffix, Qt::CaseInsensitive) && name.size() > suffix.size())
            return {name.left(name.size() - suffix.size()), name.right(suffix.size())};
    }
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0) // no dot, or a dotfile such as ".bashrc"
        return {name, QString()};
    return {name.left(dot), name.mid(dot)};
}

bool existsAnyKind(const QString &path)
{
    return QFileInfo(path).exists() || QFileInfo(path).isSymLink();
}

bool isInside(const QString &inner, const QString &outer)
{
    const QString a = QDir::cleanPath(QFileInfo(inner).absoluteFilePath());
    const QString b = QDir::cleanPath(QFileInfo(outer).absoluteFilePath());
    return a == b || a.startsWith(b + QLatin1Char('/'));
}

QString canonicalOrAbsolute(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

OpResult copyRecursive(const QString &source, const QString &target)
{
    const QFileInfo info(source);

    if (info.isSymLink()) {
        if (!QFile::link(info.symLinkTarget(), target))
            return OpResult::failure(QStringLiteral("Cannot create link %1").arg(target));
        return OpResult::success(target);
    }

    if (info.isDir()) {
        if (!QDir().mkdir(target))
            return OpResult::failure(QStringLiteral("Cannot create folder %1").arg(target));

        QDirIterator it(source, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        while (it.hasNext()) {
            it.next();
            const OpResult r = copyRecursive(it.filePath(), QDir(target).filePath(it.fileName()));
            if (!r.ok)
                return r;
        }
        QFile::setPermissions(target, info.permissions());
        return OpResult::success(target);
    }

    if (!QFile::copy(source, target))
        return OpResult::failure(QStringLiteral("Cannot copy %1").arg(info.fileName()));
    return OpResult::success(target);
}

} // namespace

QString FileOperations::uniqueName(const QString &dir, const QString &name)
{
    const QDir d(dir);
    if (!existsAnyKind(d.filePath(name)))
        return name;

    const auto [base, suffix] = splitName(name);
    for (int i = 1; i < 100000; ++i) {
        const QString candidate = QStringLiteral("%1 (%2)%3").arg(base).arg(i).arg(suffix);
        if (!existsAnyKind(d.filePath(candidate)))
            return candidate;
    }
    return name;
}

bool FileOperations::isValidName(const QString &name, QString *reason)
{
    auto fail = [&](const QString &why) {
        if (reason)
            *reason = why;
        return false;
    };

    if (name.trimmed().isEmpty())
        return fail(QStringLiteral("The name cannot be empty."));
    if (name == QLatin1String(".") || name == QLatin1String(".."))
        return fail(QStringLiteral("This name is reserved."));
    if (name.contains(QLatin1Char('/')) || name.contains(QChar(0)))
        return fail(QStringLiteral("The name cannot contain \"/\"."));
    if (name.toUtf8().size() > 255)
        return fail(QStringLiteral("The name is too long."));
    return true;
}

OpResult FileOperations::copy(const QString &source, const QString &targetDir)
{
    const QFileInfo src(source);
    if (!src.exists() && !src.isSymLink())
        return OpResult::failure(QStringLiteral("%1 does not exist.").arg(source));
    if (!QFileInfo(targetDir).isDir())
        return OpResult::failure(QStringLiteral("%1 is not a folder.").arg(targetDir));
    if (src.isDir() && !src.isSymLink() && isInside(canonicalOrAbsolute(targetDir), canonicalOrAbsolute(source)))
        return OpResult::failure(QStringLiteral("A folder cannot be copied into itself."));

    const QString name = uniqueName(targetDir, src.fileName());
    return copyRecursive(source, QDir(targetDir).filePath(name));
}

OpResult FileOperations::move(const QString &source, const QString &targetDir)
{
    const QFileInfo src(source);
    if (!src.exists() && !src.isSymLink())
        return OpResult::failure(QStringLiteral("%1 does not exist.").arg(source));
    if (!QFileInfo(targetDir).isDir())
        return OpResult::failure(QStringLiteral("%1 is not a folder.").arg(targetDir));
    if (src.isDir() && !src.isSymLink() && isInside(canonicalOrAbsolute(targetDir), canonicalOrAbsolute(source)))
        return OpResult::failure(QStringLiteral("A folder cannot be moved into itself."));

    // Moving to where it already is changes nothing.
    if (canonicalOrAbsolute(src.absolutePath()) == canonicalOrAbsolute(targetDir))
        return OpResult::success(src.absoluteFilePath());

    const QString target = QDir(targetDir).filePath(uniqueName(targetDir, src.fileName()));
    if (QDir().rename(source, target))
        return OpResult::success(target);

    // rename() fails across file systems: copy, then remove the original.
    const OpResult copied = copyRecursive(source, target);
    if (!copied.ok)
        return copied;
    const OpResult removed = remove(source);
    if (!removed.ok)
        return OpResult::failure(QStringLiteral("Copied, but could not remove the original: %1").arg(removed.error));
    return OpResult::success(target);
}

OpResult FileOperations::rename(const QString &path, const QString &newName)
{
    QString why;
    if (!isValidName(newName, &why))
        return OpResult::failure(why);

    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink())
        return OpResult::failure(QStringLiteral("%1 does not exist.").arg(path));
    if (info.fileName() == newName)
        return OpResult::success(info.absoluteFilePath());

    const QString target = QDir(info.absolutePath()).filePath(newName);
    if (existsAnyKind(target))
        return OpResult::failure(QStringLiteral("\"%1\" already exists.").arg(newName));
    if (!QDir().rename(path, target))
        return OpResult::failure(QStringLiteral("Cannot rename \"%1\".").arg(info.fileName()));
    return OpResult::success(target);
}

namespace {
bool trashFolderUsable(const QString &trashRoot)
{
    return QDir().mkpath(trashRoot + QStringLiteral("/files")) && QDir().mkpath(trashRoot + QStringLiteral("/info"))
           && QFileInfo(trashRoot + QStringLiteral("/files")).isWritable()
           && QFileInfo(trashRoot + QStringLiteral("/info")).isWritable();
}
} // namespace

bool FileOperations::canTrash(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink())
        return false;
    const QStorageInfo where(info.absolutePath());
    if (!where.isValid() || where.rootPath().isEmpty())
        return false;

    // Files on the same disk as the home folder go to the home trash ...
    const QString homeTrash = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash");
    if (trashFolderUsable(homeTrash) && QStorageInfo(homeTrash).rootPath() == where.rootPath())
        return true;

#ifdef Q_OS_UNIX
    // ... other disks use "<disk>/.Trash-<uid>".
    const QString diskTrash = QDir::cleanPath(where.rootPath() + QStringLiteral("/.Trash-") + QString::number(getuid()));
    return trashFolderUsable(diskTrash);
#else
    return false;
#endif
}

OpResult FileOperations::trash(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink())
        return OpResult::failure(QStringLiteral("%1 does not exist.").arg(path));
    if (!canTrash(path))
        return OpResult::failure(QStringLiteral("There is no usable trash for \"%1\". Delete it permanently instead.").arg(info.fileName()));

    QString pathInTrash;
    if (!QFile::moveToTrash(path, &pathInTrash))
        return OpResult::failure(QStringLiteral("Cannot move \"%1\" to the trash.").arg(info.fileName()));
    return OpResult::success(pathInTrash);
}

OpResult FileOperations::remove(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink())
        return OpResult::failure(QStringLiteral("%1 does not exist.").arg(path));

    const bool ok = (info.isDir() && !info.isSymLink()) ? QDir(path).removeRecursively()
                                                        : QFile::remove(path);
    return ok ? OpResult::success() : OpResult::failure(QStringLiteral("Cannot delete \"%1\".").arg(info.fileName()));
}

OpResult FileOperations::createFolder(const QString &dir, const QString &name)
{
    QString why;
    if (!isValidName(name, &why))
        return OpResult::failure(why);
    if (!QFileInfo(dir).isDir())
        return OpResult::failure(QStringLiteral("%1 is not a folder.").arg(dir));

    const QString target = QDir(dir).filePath(name);
    if (existsAnyKind(target))
        return OpResult::failure(QStringLiteral("\"%1\" already exists.").arg(name));
    if (!QDir().mkdir(target))
        return OpResult::failure(QStringLiteral("Cannot create the folder \"%1\".").arg(name));
    return OpResult::success(target);
}

OpResult FileOperations::createFile(const QString &dir, const QString &name)
{
    QString why;
    if (!isValidName(name, &why))
        return OpResult::failure(why);
    if (!QFileInfo(dir).isDir())
        return OpResult::failure(QStringLiteral("%1 is not a folder.").arg(dir));

    const QString target = QDir(dir).filePath(name);
    if (existsAnyKind(target))
        return OpResult::failure(QStringLiteral("\"%1\" already exists.").arg(name));

    QFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        return OpResult::failure(QStringLiteral("Cannot create the file \"%1\".").arg(name));
    return OpResult::success(target);
}

qint64 FileOperations::totalSize(const QString &path, const std::atomic_bool *cancel)
{
    const QFileInfo info(path);
    if (info.isSymLink())
        return 0;
    if (!info.isDir())
        return info.size();

    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (cancel && cancel->load())
            break;
        it.next();
        if (!it.fileInfo().isSymLink())
            total += it.fileInfo().size();
    }
    return total;
}

QString FileOperations::humanSize(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

} // namespace colibry::fm
