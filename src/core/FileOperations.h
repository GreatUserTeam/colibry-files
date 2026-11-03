#pragma once

#include <QString>
#include <QtGlobal>

#include <atomic>

namespace colibry::fm {

/// Outcome of a file operation. `path` is the resulting location on success.
struct OpResult
{
    bool ok = false;
    QString path;
    QString error;

    static OpResult success(const QString &path = {}) { return {true, path, {}}; }
    static OpResult failure(const QString &error) { return {false, {}, error}; }
};

/**
 * File operations used by the file manager. No UI, no dialogs.
 *
 * Rules shared by all operations:
 *  - nothing is ever overwritten silently: a name clash produces "name (1).ext";
 *  - a folder can never be copied or moved into itself;
 *  - symbolic links are copied as links and never followed when deleting or measuring.
 */
class FileOperations
{
public:
    /// `name` if free inside `dir`, otherwise "name (1).ext", "name (2).ext", ...
    static QString uniqueName(const QString &dir, const QString &name);

    static bool isValidName(const QString &name, QString *reason = nullptr);

    static OpResult copy(const QString &source, const QString &targetDir);
    static OpResult move(const QString &source, const QString &targetDir);
    static OpResult rename(const QString &path, const QString &newName);
    /// True when a trash folder that Qt can really use exists or can be created for this path.
    /// Qt 6.4 loops forever inside QFile::moveToTrash when it cannot set one up, so this is
    /// checked first and trash() refuses (with a message) instead of risking a frozen window.
    static bool canTrash(const QString &path);
    static OpResult trash(const QString &path);
    static OpResult remove(const QString &path);
    static OpResult createFolder(const QString &dir, const QString &name);
    static OpResult createFile(const QString &dir, const QString &name);

    /// Total size of a file, or of everything inside a folder (links not followed).
    /// `cancel` may be set from another thread to stop a long scan early.
    static qint64 totalSize(const QString &path, const std::atomic_bool *cancel = nullptr);

    /// "1.5 MB", "832 bytes", ...
    static QString humanSize(qint64 bytes);
};

} // namespace colibry::fm
