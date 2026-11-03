#include "MainWindow.h"
#include "core/FileClipboard.h"
#include "colibry/Keys.h"
#include "colibry/SettingsStore.h"

#include <QFile>
#include <QFileSystemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

using namespace colibry;
using namespace colibry::fm;

namespace {
void touch(const QString &path, const QByteArray &data = "x")
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}
} // namespace

class FileManagerUiTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void init()
    {
        QVERIFY(m_tmp.isValid());
        m_root = QFileInfo(m_tmp.path()).canonicalFilePath();
        QDir(m_root).removeRecursively();
        QDir().mkpath(m_root + QStringLiteral("/docs/inner"));
        QDir().mkpath(m_root + QStringLiteral("/other"));
        touch(m_root + QStringLiteral("/a.txt"), "aaa");
        touch(m_root + QStringLiteral("/b.txt"), "bbbbbb");
        touch(m_root + QStringLiteral("/.hidden"), "h");
        FileClipboard::clear();
        m_store = std::make_unique<SettingsStore>(m_tmp.filePath(QStringLiteral("../") + QString::number(qrand_id()) + QStringLiteral(".ini")));
    }

    void opensStartFolderAndListsItems()
    {
        MainWindow w(m_root, *m_store);
        QCOMPARE(w.currentPath(), m_root);
        QTRY_COMPARE(w.model()->rowCount(w.model()->index(m_root)), 4); // docs, other, a.txt, b.txt
    }

    void hiddenFilesAreOptIn()
    {
        MainWindow w(m_root, *m_store);
        QTRY_COMPARE(w.model()->rowCount(w.model()->index(m_root)), 4);
        w.setShowHidden(true);
        QTRY_COMPARE(w.model()->rowCount(w.model()->index(m_root)), 5);
        QVERIFY(w.showsHidden());
        w.setShowHidden(false);
        QTRY_COMPARE(w.model()->rowCount(w.model()->index(m_root)), 4);
    }

    void navigationAndHistory()
    {
        MainWindow w(m_root, *m_store);
        QVERIFY(w.navigateTo(m_root + QStringLiteral("/docs")));
        QVERIFY(w.navigateTo(m_root + QStringLiteral("/docs/inner")));
        QCOMPARE(w.currentPath(), m_root + QStringLiteral("/docs/inner"));

        QVERIFY(w.goUp());
        QCOMPARE(w.currentPath(), m_root + QStringLiteral("/docs"));
        // "Up" is a navigation too: history is root, docs, inner, docs.
        QVERIFY(w.goBack());
        QCOMPARE(w.currentPath(), m_root + QStringLiteral("/docs/inner"));
        QVERIFY(w.goBack());
        QCOMPARE(w.currentPath(), m_root + QStringLiteral("/docs"));
        QVERIFY(w.goBack());
        QCOMPARE(w.currentPath(), m_root);
        QVERIFY(!w.goBack());
        QVERIFY(w.goForward());
        QCOMPARE(w.currentPath(), m_root + QStringLiteral("/docs"));
    }

    void refusesToOpenInvalidFolders()
    {
        MainWindow w(m_root, *m_store);
        QVERIFY(!w.navigateTo(m_root + QStringLiteral("/nope")));
        QVERIFY(!w.navigateTo(m_root + QStringLiteral("/a.txt"))); // a file, not a folder
        QVERIFY(!w.navigateTo(QString()));
        QCOMPARE(w.currentPath(), m_root);
    }

    void fallsBackToHomeForBadStartFolder()
    {
        MainWindow w(m_root + QStringLiteral("/does-not-exist"), *m_store);
        QCOMPARE(w.currentPath(), QDir::cleanPath(QDir::homePath()));
    }

    void createFolderSelectsIt()
    {
        MainWindow w(m_root, *m_store);
        const OpResult r = w.createFolder(QStringLiteral("fresh"));
        QVERIFY(r.ok);
        QVERIFY(QFileInfo(m_root + QStringLiteral("/fresh")).isDir());
        QTRY_COMPARE(w.selectedPaths(), QStringList{m_root + QStringLiteral("/fresh")});
        QVERIFY(!w.createFolder(QStringLiteral("fresh")).ok);
        QVERIFY(!w.createFolder(QStringLiteral("a/b")).ok);
    }

    void createFileAndRename()
    {
        MainWindow w(m_root, *m_store);
        QVERIFY(w.createFile(QStringLiteral("note.txt")).ok);
        QTRY_COMPARE(w.selectedPaths(), QStringList{m_root + QStringLiteral("/note.txt")});

        QVERIFY(w.renameSelected(QStringLiteral("memo.txt")).ok);
        QVERIFY(QFileInfo::exists(m_root + QStringLiteral("/memo.txt")));
        QVERIFY(!QFileInfo::exists(m_root + QStringLiteral("/note.txt")));
        QTRY_COMPARE(w.selectedPaths(), QStringList{m_root + QStringLiteral("/memo.txt")});

        QVERIFY(!w.renameSelected(QStringLiteral("a.txt")).ok); // clash
        QVERIFY(!w.renameSelected(QString()).ok);
    }

    void renameNeedsExactlyOneSelection()
    {
        MainWindow w(m_root, *m_store);
        QVERIFY(!w.renameSelected(QStringLiteral("x")).ok); // nothing selected
    }

    void copyAndPasteIntoAnotherFolder()
    {
        MainWindow w(m_root, *m_store);
        w.selectPath(m_root + QStringLiteral("/a.txt"));
        QTRY_COMPARE(w.selectedPaths().size(), 1);
        w.copySelection();

        QVERIFY(w.navigateTo(m_root + QStringLiteral("/other")));
        const PasteResult r = w.pasteHere();
        QCOMPARE(r.done, 1);
        QVERIFY(r.errors.isEmpty());
        QVERIFY(QFileInfo::exists(m_root + QStringLiteral("/other/a.txt")));
        QVERIFY(QFileInfo::exists(m_root + QStringLiteral("/a.txt"))); // copy keeps the original
        QVERIFY(FileClipboard::hasFiles());                            // copy can be pasted again
    }

    void cutAndPasteMovesAndClearsClipboard()
    {
        MainWindow w(m_root, *m_store);
        w.selectPath(m_root + QStringLiteral("/b.txt"));
        QTRY_COMPARE(w.selectedPaths().size(), 1);
        w.cutSelection();

        QVERIFY(w.navigateTo(m_root + QStringLiteral("/other")));
        QCOMPARE(w.pasteHere().done, 1);
        QVERIFY(QFileInfo::exists(m_root + QStringLiteral("/other/b.txt")));
        QVERIFY(!QFileInfo::exists(m_root + QStringLiteral("/b.txt")));
        QVERIFY(!FileClipboard::hasFiles());
    }

    void pasteReportsErrorsForFolderIntoItself()
    {
        MainWindow w(m_root, *m_store);
        w.selectPath(m_root + QStringLiteral("/docs"));
        QTRY_COMPARE(w.selectedPaths().size(), 1);
        w.copySelection();
        QVERIFY(w.navigateTo(m_root + QStringLiteral("/docs/inner")));
        const PasteResult r = w.pasteHere();
        QCOMPARE(r.done, 0);
        QCOMPARE(r.errors.size(), 1);
    }

    void deletePermanentlyRemovesSelection()
    {
        MainWindow w(m_root, *m_store);
        w.selectPath(m_root + QStringLiteral("/a.txt"));
        QTRY_COMPARE(w.selectedPaths().size(), 1);
        const PasteResult r = w.deleteSelectionPermanently();
        QCOMPARE(r.done, 1);
        QVERIFY(!QFileInfo::exists(m_root + QStringLiteral("/a.txt")));
        QVERIFY(QFileInfo::exists(m_root + QStringLiteral("/b.txt")));
    }

    void trashMovesAwayFromFolder()
    {
        MainWindow w(m_root, *m_store);
        w.selectPath(m_root + QStringLiteral("/b.txt"));
        QTRY_COMPARE(w.selectedPaths().size(), 1);
        const bool trashWorks = FileOperations::canTrash(m_root + QStringLiteral("/b.txt"));
        const PasteResult r = w.trashSelection();
        if (!trashWorks) {
            QCOMPARE(r.done, 0);
            QCOMPARE(r.errors.size(), 1);
            QVERIFY(QFileInfo::exists(m_root + QStringLiteral("/b.txt"))); // refused, nothing lost
            QSKIP("No usable trash folder in this environment.", SkipSingle);
        }
        QVERIFY2(r.errors.isEmpty(), qPrintable(r.errors.join(QLatin1Char(' '))));
        QVERIFY(!QFileInfo::exists(m_root + QStringLiteral("/b.txt")));
    }

    void filterNarrowsListAndResetsOnNavigation()
    {
        MainWindow w(m_root, *m_store);
        QTRY_COMPARE(w.model()->rowCount(w.model()->index(m_root)), 4);
        w.setFilterText(QStringLiteral("a.t"));
        QTRY_COMPARE(w.model()->rowCount(w.model()->index(m_root)), 1);
        QVERIFY(w.navigateTo(m_root + QStringLiteral("/docs")));
        QVERIFY(w.navigateTo(m_root));
        QTRY_COMPARE(w.model()->rowCount(w.model()->index(m_root)), 4);
    }

    void viewModeAndHiddenPersistBetweenWindows()
    {
        {
            MainWindow w(m_root, *m_store);
            w.setIconView(true);
            w.setShowHidden(true);
        }
        MainWindow again(m_root, *m_store);
        QVERIFY(again.isIconView());
        QVERIFY(again.showsHidden());
    }

private:
    static int qrand_id()
    {
        static int n = 0;
        return ++n;
    }

    QTemporaryDir m_tmp;
    QString m_root;
    std::unique_ptr<SettingsStore> m_store;
};

QTEST_MAIN(FileManagerUiTest)
#include "test_ui.moc"
