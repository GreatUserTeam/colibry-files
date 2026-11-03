#include "MainWindow.h"
#include "colibry/SettingsStore.h"
#include "colibry/ThemeSync.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("colibry-files"));
    QApplication::setApplicationDisplayName(QStringLiteral("Files"));
    QApplication::setApplicationVersion(QStringLiteral(Colibry_APPS_VERSION));
    QApplication::setOrganizationName(QStringLiteral("GreatUserTeam"));
    QApplication::setDesktopFileName(QStringLiteral("colibry-files"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("File manager for the Great User System"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("folder"), QStringLiteral("Folder to open (default: home)."));
    parser.process(app);

    colibry::SettingsStore store;
    colibry::ThemeSync themeSync(app, store);

    const QStringList args = parser.positionalArguments();
    colibry::fm::MainWindow window(args.isEmpty() ? QDir::homePath() : args.first(), store);
    window.show();
    return QApplication::exec();
}
