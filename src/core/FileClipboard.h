#pragma once

#include <QStringList>

namespace colibry::fm {

struct ClipboardContent
{
    QStringList paths;
    bool cut = false; ///< true: paste moves the files, false: paste copies them
};

/// Copy / cut clipboard shared through the system clipboard (URI list + a "cut" marker),
/// so other applications can paste the files too.
class FileClipboard
{
public:
    static void set(const QStringList &paths, bool cut);
    static ClipboardContent get();
    static bool hasFiles();
    static void clear();
};

} // namespace colibry::fm
