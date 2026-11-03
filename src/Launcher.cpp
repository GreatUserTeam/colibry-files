#include "Launcher.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

namespace colibry::fm {

QString Launcher::findViewer()
{
    const QString name = QStringLiteral("colibry-viewer");
    const QString beside = QStandardPaths::findExecutable(name, {QCoreApplication::applicationDirPath()});
    if (!beside.isEmpty())
        return beside;
    return QStandardPaths::findExecutable(name);
}

bool Launcher::openFile(const QString &path)
{
    const QString viewer = findViewer();
    if (!viewer.isEmpty() && QProcess::startDetached(viewer, {path}))
        return true;
    return openWithSystem(path);
}

bool Launcher::openWithSystem(const QString &path)
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

} // namespace colibry::fm
