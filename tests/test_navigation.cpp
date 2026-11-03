#include "core/FileClipboard.h"
#include "core/NavigationHistory.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace colibry::fm;

class NavigationTest : public QObject
{
    Q_OBJECT

private slots:
    void startsEmpty()
    {
        NavigationHistory h;
        QVERIFY(!h.canGoBack());
        QVERIFY(!h.canGoForward());
        QVERIFY(h.current().isEmpty());
        QVERIFY(h.back().isEmpty());
        QVERIFY(h.forward().isEmpty());
    }

    void backAndForward()
    {
        NavigationHistory h;
        h.visit(QStringLiteral("/a"));
        h.visit(QStringLiteral("/b"));
        h.visit(QStringLiteral("/c"));
        QCOMPARE(h.current(), QStringLiteral("/c"));
        QCOMPARE(h.back(), QStringLiteral("/b"));
        QCOMPARE(h.back(), QStringLiteral("/a"));
        QVERIFY(!h.canGoBack());
        QVERIFY(h.canGoForward());
        QCOMPARE(h.forward(), QStringLiteral("/b"));
        QCOMPARE(h.forward(), QStringLiteral("/c"));
        QVERIFY(!h.canGoForward());
    }

    void newVisitDropsForwardList()
    {
        NavigationHistory h;
        h.visit(QStringLiteral("/a"));
        h.visit(QStringLiteral("/b"));
        h.back();
        h.visit(QStringLiteral("/x"));
        QVERIFY(!h.canGoForward());
        QCOMPARE(h.back(), QStringLiteral("/a"));
    }

    void revisitingCurrentFolderIsIgnored()
    {
        NavigationHistory h;
        h.visit(QStringLiteral("/a"));
        h.visit(QStringLiteral("/a"));
        QVERIFY(!h.canGoBack());
    }

    void historyIsBounded()
    {
        NavigationHistory h;
        for (int i = 0; i < 500; ++i)
            h.visit(QStringLiteral("/d%1").arg(i));
        int steps = 0;
        while (h.canGoBack()) {
            h.back();
            ++steps;
        }
        QVERIFY(steps <= 200);
        QCOMPARE(h.current(), QStringLiteral("/d300"));
    }

    void clipboardRoundTripsCopyAndCut()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath(QStringLiteral("a.txt"));
        QFile f(a);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();

        FileClipboard::set({a}, false);
        auto c = FileClipboard::get();
        QCOMPARE(c.paths, QStringList{a});
        QVERIFY(!c.cut);

        FileClipboard::set({a}, true);
        QVERIFY(FileClipboard::get().cut);
        QVERIFY(FileClipboard::hasFiles());

        FileClipboard::clear();
        QVERIFY(!FileClipboard::hasFiles());
    }

    void clipboardIgnoresFilesThatDisappeared()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath(QStringLiteral("gone.txt"));
        FileClipboard::set({a}, false);
        QVERIFY(FileClipboard::get().paths.isEmpty());
    }
};

QTEST_MAIN(NavigationTest)
#include "test_navigation.moc"
