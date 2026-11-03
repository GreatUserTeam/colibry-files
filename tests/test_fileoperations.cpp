#include "core/FileOperations.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

using namespace colibry::fm;

namespace {
void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}
QByteArray readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
} // namespace

class FileOperationsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void init()
    {
        QVERIFY(m_tmp.isValid());
        m_root = m_tmp.path();
        QDir(m_root).removeRecursively();
        QDir().mkpath(m_root);
    }

    // ---- uniqueName ---------------------------------------------------
    void uniqueNameKeepsFreeNames()
    {
        QCOMPARE(FileOperations::uniqueName(m_root, QStringLiteral("a.txt")), QStringLiteral("a.txt"));
    }

    void uniqueNameAddsCounterBeforeExtension()
    {
        writeFile(m_root + QStringLiteral("/a.txt"), "x");
        QCOMPARE(FileOperations::uniqueName(m_root, QStringLiteral("a.txt")), QStringLiteral("a (1).txt"));
        writeFile(m_root + QStringLiteral("/a (1).txt"), "x");
        QCOMPARE(FileOperations::uniqueName(m_root, QStringLiteral("a.txt")), QStringLiteral("a (2).txt"));
    }

    void uniqueNameHandlesDoubleSuffixAndDotfiles()
    {
        writeFile(m_root + QStringLiteral("/backup.tar.gz"), "x");
        QCOMPARE(FileOperations::uniqueName(m_root, QStringLiteral("backup.tar.gz")),
                 QStringLiteral("backup (1).tar.gz"));
        writeFile(m_root + QStringLiteral("/.bashrc"), "x");
        QCOMPARE(FileOperations::uniqueName(m_root, QStringLiteral(".bashrc")), QStringLiteral(".bashrc (1)"));
        QDir(m_root).mkdir(QStringLiteral("folder"));
        QCOMPARE(FileOperations::uniqueName(m_root, QStringLiteral("folder")), QStringLiteral("folder (1)"));
    }

    // ---- names --------------------------------------------------------
    void validatesNames()
    {
        QString why;
        QVERIFY(FileOperations::isValidName(QStringLiteral("report 2026.pdf")));
        QVERIFY(FileOperations::isValidName(QStringLiteral("файл.txt")));
        QVERIFY(!FileOperations::isValidName(QString(), &why));
        QVERIFY(!why.isEmpty());
        QVERIFY(!FileOperations::isValidName(QStringLiteral("   ")));
        QVERIFY(!FileOperations::isValidName(QStringLiteral(".")));
        QVERIFY(!FileOperations::isValidName(QStringLiteral("..")));
        QVERIFY(!FileOperations::isValidName(QStringLiteral("a/b")));
        QVERIFY(!FileOperations::isValidName(QString(300, QLatin1Char('x'))));
    }

    // ---- create -------------------------------------------------------
    void createsFolderAndFile()
    {
        const auto folder = FileOperations::createFolder(m_root, QStringLiteral("docs"));
        QVERIFY(folder.ok);
        QVERIFY(QFileInfo(folder.path).isDir());

        const auto file = FileOperations::createFile(m_root, QStringLiteral("a.txt"));
        QVERIFY(file.ok);
        QVERIFY(QFileInfo(file.path).isFile());
        QCOMPARE(QFileInfo(file.path).size(), 0);
    }

    void createRefusesExistingAndInvalid()
    {
        QVERIFY(FileOperations::createFolder(m_root, QStringLiteral("docs")).ok);
        QVERIFY(!FileOperations::createFolder(m_root, QStringLiteral("docs")).ok);
        QVERIFY(!FileOperations::createFile(m_root, QStringLiteral("docs")).ok);
        QVERIFY(!FileOperations::createFile(m_root, QStringLiteral("a/b")).ok);
        QVERIFY(!FileOperations::createFolder(m_root + QStringLiteral("/missing"), QStringLiteral("x")).ok);
    }

    void createFileDoesNotTruncateExisting()
    {
        writeFile(m_root + QStringLiteral("/keep.txt"), "important");
        QVERIFY(!FileOperations::createFile(m_root, QStringLiteral("keep.txt")).ok);
        QCOMPARE(readFile(m_root + QStringLiteral("/keep.txt")), QByteArray("important"));
    }

    // ---- copy ---------------------------------------------------------
    void copiesFileIntoOtherFolder()
    {
        QDir(m_root).mkdir(QStringLiteral("dst"));
        writeFile(m_root + QStringLiteral("/a.txt"), "hello");
        const auto r = FileOperations::copy(m_root + QStringLiteral("/a.txt"), m_root + QStringLiteral("/dst"));
        QVERIFY(r.ok);
        QCOMPARE(readFile(r.path), QByteArray("hello"));
        QVERIFY(QFileInfo::exists(m_root + QStringLiteral("/a.txt")));
    }

    void copyIntoSameFolderMakesNumberedCopy()
    {
        writeFile(m_root + QStringLiteral("/a.txt"), "hello");
        const auto r = FileOperations::copy(m_root + QStringLiteral("/a.txt"), m_root);
        QVERIFY(r.ok);
        QCOMPARE(QFileInfo(r.path).fileName(), QStringLiteral("a (1).txt"));
    }

    void copiesFolderRecursivelyWithPermissionsAndHiddenFiles()
    {
        QDir(m_root).mkpath(QStringLiteral("src/sub/deep"));
        writeFile(m_root + QStringLiteral("/src/one.txt"), "1");
        writeFile(m_root + QStringLiteral("/src/sub/two.txt"), "22");
        writeFile(m_root + QStringLiteral("/src/sub/deep/.hidden"), "333");
        QFile::setPermissions(m_root + QStringLiteral("/src/one.txt"),
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        QDir(m_root).mkdir(QStringLiteral("dst"));

        const auto r = FileOperations::copy(m_root + QStringLiteral("/src"), m_root + QStringLiteral("/dst"));
        QVERIFY(r.ok);
        QCOMPARE(readFile(r.path + QStringLiteral("/one.txt")), QByteArray("1"));
        QCOMPARE(readFile(r.path + QStringLiteral("/sub/two.txt")), QByteArray("22"));
        QCOMPARE(readFile(r.path + QStringLiteral("/sub/deep/.hidden")), QByteArray("333"));
        QVERIFY(QFileInfo(r.path + QStringLiteral("/one.txt")).permissions().testFlag(QFileDevice::ExeOwner));
    }

    void copyKeepsSymlinksAsLinks()
    {
        QDir(m_root).mkdir(QStringLiteral("dst"));
        writeFile(m_root + QStringLiteral("/target.txt"), "t");
        QVERIFY(QFile::link(m_root + QStringLiteral("/target.txt"), m_root + QStringLiteral("/link")));
        const auto r = FileOperations::copy(m_root + QStringLiteral("/link"), m_root + QStringLiteral("/dst"));
        QVERIFY(r.ok);
        QVERIFY(QFileInfo(r.path).isSymLink());
    }

    void copyRefusesFolderIntoItself()
    {
        QDir(m_root).mkpath(QStringLiteral("a/b"));
        QVERIFY(!FileOperations::copy(m_root + QStringLiteral("/a"), m_root + QStringLiteral("/a")).ok);
        QVERIFY(!FileOperations::copy(m_root + QStringLiteral("/a"), m_root + QStringLiteral("/a/b")).ok);
        // a sibling whose name merely starts the same is fine
        QDir(m_root).mkdir(QStringLiteral("a2"));
        QVERIFY(FileOperations::copy(m_root + QStringLiteral("/a"), m_root + QStringLiteral("/a2")).ok);
    }

    void copyOfMissingSourceFails()
    {
        QVERIFY(!FileOperations::copy(m_root + QStringLiteral("/nope"), m_root).ok);
    }

    // ---- move ---------------------------------------------------------
    void movesFileAndFolder()
    {
        QDir(m_root).mkpath(QStringLiteral("dst"));
        QDir(m_root).mkpath(QStringLiteral("dir/inner"));
        writeFile(m_root + QStringLiteral("/dir/inner/f.txt"), "f");
        writeFile(m_root + QStringLiteral("/a.txt"), "a");

        QVERIFY(FileOperations::move(m_root + QStringLiteral("/a.txt"), m_root + QStringLiteral("/dst")).ok);
        QVERIFY(!QFileInfo::exists(m_root + QStringLiteral("/a.txt")));
        QVERIFY(QFileInfo::exists(m_root + QStringLiteral("/dst/a.txt")));

        const auto r = FileOperations::move(m_root + QStringLiteral("/dir"), m_root + QStringLiteral("/dst"));
        QVERIFY(r.ok);
        QVERIFY(!QFileInfo::exists(m_root + QStringLiteral("/dir")));
        QCOMPARE(readFile(r.path + QStringLiteral("/inner/f.txt")), QByteArray("f"));
    }

    void moveDoesNotOverwrite()
    {
        QDir(m_root).mkdir(QStringLiteral("dst"));
        writeFile(m_root + QStringLiteral("/a.txt"), "new");
        writeFile(m_root + QStringLiteral("/dst/a.txt"), "old");
        const auto r = FileOperations::move(m_root + QStringLiteral("/a.txt"), m_root + QStringLiteral("/dst"));
        QVERIFY(r.ok);
        QCOMPARE(QFileInfo(r.path).fileName(), QStringLiteral("a (1).txt"));
        QCOMPARE(readFile(m_root + QStringLiteral("/dst/a.txt")), QByteArray("old"));
        QCOMPARE(readFile(r.path), QByteArray("new"));
    }

    void moveToSameFolderIsNoOp()
    {
        writeFile(m_root + QStringLiteral("/a.txt"), "a");
        const auto r = FileOperations::move(m_root + QStringLiteral("/a.txt"), m_root);
        QVERIFY(r.ok);
        QCOMPARE(QFileInfo(r.path).fileName(), QStringLiteral("a.txt"));
        QVERIFY(!QFileInfo::exists(m_root + QStringLiteral("/a (1).txt")));
    }

    void moveRefusesFolderIntoItself()
    {
        QDir(m_root).mkpath(QStringLiteral("a/b"));
        QVERIFY(!FileOperations::move(m_root + QStringLiteral("/a"), m_root + QStringLiteral("/a/b")).ok);
        QVERIFY(QFileInfo(m_root + QStringLiteral("/a/b")).isDir());
    }

    // ---- rename -------------------------------------------------------
    void renamesAndRefusesClashes()
    {
        writeFile(m_root + QStringLiteral("/a.txt"), "a");
        writeFile(m_root + QStringLiteral("/b.txt"), "b");

        const auto ok = FileOperations::rename(m_root + QStringLiteral("/a.txt"), QStringLiteral("c.txt"));
        QVERIFY(ok.ok);
        QVERIFY(QFileInfo::exists(m_root + QStringLiteral("/c.txt")));
        QVERIFY(!QFileInfo::exists(m_root + QStringLiteral("/a.txt")));

        const auto clash = FileOperations::rename(m_root + QStringLiteral("/c.txt"), QStringLiteral("b.txt"));
        QVERIFY(!clash.ok);
        QCOMPARE(readFile(m_root + QStringLiteral("/b.txt")), QByteArray("b"));

        QVERIFY(!FileOperations::rename(m_root + QStringLiteral("/c.txt"), QStringLiteral("x/y")).ok);
        QVERIFY(FileOperations::rename(m_root + QStringLiteral("/c.txt"), QStringLiteral("c.txt")).ok); // same name
    }

    void renamesFolders()
    {
        QDir(m_root).mkpath(QStringLiteral("old/x"));
        const auto r = FileOperations::rename(m_root + QStringLiteral("/old"), QStringLiteral("new"));
        QVERIFY(r.ok);
        QVERIFY(QFileInfo(m_root + QStringLiteral("/new/x")).isDir());
    }

    // ---- remove -------------------------------------------------------
    void removesFilesAndFolders()
    {
        QDir(m_root).mkpath(QStringLiteral("d/e"));
        writeFile(m_root + QStringLiteral("/d/e/f"), "f");
        writeFile(m_root + QStringLiteral("/a.txt"), "a");
        QVERIFY(FileOperations::remove(m_root + QStringLiteral("/a.txt")).ok);
        QVERIFY(FileOperations::remove(m_root + QStringLiteral("/d")).ok);
        QVERIFY(!QFileInfo::exists(m_root + QStringLiteral("/d")));
        QVERIFY(!FileOperations::remove(m_root + QStringLiteral("/d")).ok);
    }

    void removeDoesNotFollowSymlinks()
    {
        QDir(m_root).mkpath(QStringLiteral("real"));
        writeFile(m_root + QStringLiteral("/real/keep.txt"), "k");
        QVERIFY(QFile::link(m_root + QStringLiteral("/real"), m_root + QStringLiteral("/linkdir")));
        QVERIFY(FileOperations::remove(m_root + QStringLiteral("/linkdir")).ok);
        QVERIFY(!QFileInfo(m_root + QStringLiteral("/linkdir")).isSymLink());
        QVERIFY(QFileInfo::exists(m_root + QStringLiteral("/real/keep.txt")));
    }

    void trashRemovesFromOriginalPlace()
    {
        writeFile(m_root + QStringLiteral("/gone.txt"), "g");
        if (!FileOperations::canTrash(m_root + QStringLiteral("/gone.txt"))) {
            // No usable trash here (e.g. a sandbox): trash() must refuse quickly instead of hanging.
            const auto refused = FileOperations::trash(m_root + QStringLiteral("/gone.txt"));
            QVERIFY(!refused.ok);
            QVERIFY(!refused.error.isEmpty());
            QVERIFY(QFileInfo::exists(m_root + QStringLiteral("/gone.txt"))); // nothing was lost
            QSKIP("No usable trash folder in this environment.", SkipSingle);
        }
        const auto r = FileOperations::trash(m_root + QStringLiteral("/gone.txt"));
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY(!QFileInfo::exists(m_root + QStringLiteral("/gone.txt")));
        QVERIFY(!r.path.isEmpty());
        QVERIFY(QFileInfo::exists(r.path));
    }

    // ---- sizes --------------------------------------------------------
    void totalSizeSumsRecursivelyAndSkipsLinks()
    {
        QDir(m_root).mkpath(QStringLiteral("d/sub"));
        writeFile(m_root + QStringLiteral("/d/a"), QByteArray(10, 'a'));
        writeFile(m_root + QStringLiteral("/d/sub/b"), QByteArray(5, 'b'));
        writeFile(m_root + QStringLiteral("/big"), QByteArray(1000, 'x'));
        QVERIFY(QFile::link(m_root + QStringLiteral("/big"), m_root + QStringLiteral("/d/link")));
        QCOMPARE(FileOperations::totalSize(m_root + QStringLiteral("/d")), 15);
        QCOMPARE(FileOperations::totalSize(m_root + QStringLiteral("/big")), 1000);
    }

    void humanSizeIsReadable()
    {
        QVERIFY(FileOperations::humanSize(0).contains(QLatin1Char('0')));
        QVERIFY(FileOperations::humanSize(1536).contains(QStringLiteral("1.5")));
    }

private:
    QTemporaryDir m_tmp;
    QString m_root;
};

QTEST_MAIN(FileOperationsTest)
#include "test_fileoperations.moc"
