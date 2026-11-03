#pragma once

#include <QString>

namespace colibry::fm {

/// Opens files in the Colibry viewer, or in the system default application as a fallback.
class Launcher
{
public:
    /// Full path of the colibry-viewer executable, or an empty string when it is not installed.
    static QString findViewer();

    /// Opens a file: colibry-viewer when available, otherwise the system default.
    static bool openFile(const QString &path);

    /// Always uses the system default application.
    static bool openWithSystem(const QString &path);
};

} // namespace colibry::fm
