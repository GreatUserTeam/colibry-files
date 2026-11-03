#pragma once

#include <QFileIconProvider>

namespace colibry::fm {

/// Uses the icon theme when it has an icon, and the built-in style icons otherwise,
/// so files and folders never appear without a picture (minimal systems ship no icon theme).
class FileIcons : public QFileIconProvider
{
public:
    using QFileIconProvider::icon;

    /// One provider for the whole process. QFileSystemModel does not own its provider and
    /// reads it from a worker thread until the very end of its life, so this object is
    /// intentionally never destroyed.
    static FileIcons *shared();

    QIcon icon(IconType type) const override;
    QIcon icon(const QFileInfo &info) const override;

private:
    static QIcon fallback(bool isDir, bool isLink);
};

} // namespace colibry::fm
