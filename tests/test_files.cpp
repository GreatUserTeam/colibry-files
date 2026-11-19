// Files tests: core logic without a window, then the window logic headless.
#include "FilesApp.hpp"

#include <colibry/process.hpp>

#include "check.hpp"

#include <fstream>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace colibry::files;

static void write(const fs::path& p, const std::string& text) {
	fs::create_directories(p.parent_path());
	std::ofstream(p, std::ios::binary) << text;
}

static int testSelection() {
	Selection s;
	s.reset(6);
	s.selectOnly(1);
	s.toggle(3);
	EXPECT(s.count() == 2 && s.isSelected(1) && s.isSelected(3));
	s.selectRange(5);  // anchor is 3 (last toggled)
	EXPECT(s.count() == 3 && s.isSelected(3) && s.isSelected(4) && s.isSelected(5) && !s.isSelected(1));
	s.invert();
	EXPECT(s.count() == 3 && s.isSelected(0));
	s.selectAll();
	EXPECT(s.count() == 6);
	s.clear();
	EXPECT(s.count() == 0);
	std::vector<std::string> names = {"a.png", "B.PNG", "c.txt", "d.jpg", "a1"};
	EXPECT(s.selectGlob(names, "*.png") == 2);  // case-insensitive
	EXPECT(s.selectGlob(names, "?.jpg", true) == 1 && s.count() == 3);
	EXPECT(Selection::globMatch("a[0-9]", "a1") && !Selection::globMatch("a[!0-9]", "a1"));
	EXPECT(!Selection::globMatch("*.txt", "c.txtx"));
	return 0;
}

static int testNavigation() {
	Navigation n;
	EXPECT(!n.canGoBack() && !n.canGoForward());
	n.visit("/a"); n.visit("/b"); n.visit("/c");
	EXPECT(n.back() == "/b" && n.back() == "/a" && !n.canGoBack());
	EXPECT(n.forward() == "/b");
	n.visit("/d");  // drops the forward part
	EXPECT(!n.canGoForward() && n.current() == "/d");
	Clipboard c;
	c.set({"/x"}, true);
	EXPECT(c.hasFiles() && c.cut);
	c.clear();
	EXPECT(!c.hasFiles());
	return 0;
}

static int testFileOps(const fs::path& root) {
	namespace ops = fileops;
	write(root / "a.txt", "hello");
	EXPECT(ops::uniqueName(root, "a.txt") == "a (1).txt");
	EXPECT(ops::uniqueName(root, "free.txt") == "free.txt");
	EXPECT(!ops::isValidName("a/b") && !ops::isValidName("..") && !ops::isValidName("") && ops::isValidName("ok name.txt"));

	fs::create_directories(root / "dir");
	auto c = ops::copy(root / "a.txt", root / "dir");
	EXPECT(c.ok && fs::exists(root / "dir" / "a.txt"));
	c = ops::copy(root / "a.txt", root / "dir");  // clash: nothing is overwritten
	EXPECT(c.ok && c.path.filename() == "a (1).txt");
	EXPECT(!ops::copy(root / "dir", root / "dir").ok);          // into itself
	EXPECT(!ops::move(root / "dir", root / "dir" / "sub").ok);  // into its own child
	auto m = ops::move(root / "a.txt", root / "dir");
	EXPECT(m.ok && !fs::exists(root / "a.txt") && m.path.filename() == "a (2).txt");
	auto r = ops::rename(root / "dir" / "a.txt", "renamed.txt");
	EXPECT(r.ok && fs::exists(root / "dir" / "renamed.txt"));
	EXPECT(!ops::rename(root / "dir" / "renamed.txt", "../x").ok);
	EXPECT(ops::createFolder(root, "f1").ok && ops::createFile(root, "n.txt").ok);
	EXPECT(ops::totalSize(root / "dir") == 15);  // three 5-byte copies
	EXPECT(ops::humanSize(0) == "0 bytes" || !ops::humanSize(0).empty());
	EXPECT(ops::humanSize(1536).find("1.5") != std::string::npos);
	auto list = ops::list(root, false);
	EXPECT(!list.empty() && list.front().isDir);  // folders first
	EXPECT(ops::remove(root / "dir").ok && !fs::exists(root / "dir"));
	EXPECT(!ops::remove("/").ok && !ops::remove("").ok);
	return 0;
}

static int testProperties(const fs::path& root) {
	write(root / "p" / "x.txt", "12345");
	write(root / "p" / "sub" / "y.txt", "123");
	Properties f = describe(root / "p" / "x.txt");
	EXPECT(f.ok && f.size == 5 && f.type == "Text file" && f.permissions.size() == 9);
	Properties d = describe(root / "p");
	EXPECT(d.ok && d.type == "Folder" && d.size == 8 && d.items == 3);
	EXPECT(d.text().find("3 items") != std::string::npos);
	EXPECT(!describe(root / "missing").ok);
	EXPECT(permissionString(fs::perms::owner_all | fs::perms::group_read) == "rwxr-----");
	return 0;
}

static int testArchive(const fs::path& root) {
	EXPECT(archive::isArchive("a.tar.gz") && archive::isArchive("B.TGZ") && !archive::isArchive("a.zip"));
	EXPECT(archive::isSafeEntryName("a/b.txt") && !archive::isSafeEntryName("/etc/passwd") && !archive::isSafeEntryName("a/../../x"));
	auto args = archive::createArgs("/o/a.tar.gz", "/p", {"-weird", "b"});
	EXPECT(args[0] == "tar" && args[5] == "--" && args[6] == "-weird");  // names after "--"

	if (!colibry::programExists("tar")) { std::printf("tar missing: archive run skipped\n"); return 0; }
	write(root / "ar" / "in" / "one.txt", "data");
	write(root / "ar" / "in" / "d" / "two.txt", "more");
	auto made = archive::create({root / "ar" / "in"}, root / "ar");
	EXPECT(made.ok && made.path.filename() == "in.tar.gz");
	auto again = archive::create({root / "ar" / "in"}, root / "ar");
	EXPECT(again.ok && again.path.filename() == "in (1).tar.gz");  // never overwrites
	auto out = archive::extract(made.path, root / "ar");
	EXPECT(out.ok && fs::exists(out.path / "in" / "d" / "two.txt"));

	// a hostile archive is refused: its entry climbs out of the folder
	write(root / "evil" / "x.txt", "x");
	auto bad = colibry::runProcess({"tar", "-cf", (root / "evil.tar").string(), "--transform=s|x.txt|../escaped.txt|", "-C", (root / "evil").string(), "x.txt"});
	if (bad.started && bad.exitCode == 0) {
		auto refused = archive::extract(root / "evil.tar", root / "evil");
		EXPECT(!refused.ok && !fs::exists(root / "escaped.txt"));
	}
	return 0;
}

static int testConvert(const fs::path& root) {
	using namespace convert;
	EXPECT(familyOf("PNG") == Family::Image && familyOf(".mp3") == Family::Audio && familyOf("xyz") == Family::None);
	auto t = targetsFor("png");
	EXPECT(std::find(t.begin(), t.end(), "jpg") != t.end() && std::find(t.begin(), t.end(), "png") == t.end());

	write(root / "c" / "pic.png", "x");
	auto p = plan(root / "c" / "pic.png", "JPG", root / "c", "magick", false);
	EXPECT(p.ok && p.argv.size() == 3 && p.argv[0] == "magick" && p.output.filename() == "pic.jpg");
	EXPECT(!plan(root / "c" / "pic.png", "jpg", root / "c", "", false).ok);          // no ImageMagick
	EXPECT(!plan(root / "c" / "pic.png", "mp3", root / "c", "magick", true).ok);     // wrong family
	EXPECT(!plan(root / "c" / "pic.xyz", "png", root / "c", "magick", true).ok);
	write(root / "c" / "pic.jpg", "y");
	EXPECT(plan(root / "c" / "pic.png", "jpg", root / "c", "magick", false).output.filename() == "pic (1).jpg");  // no overwrite
	auto v = plan(root / "c" / "movie.mp4", "mp3", root / "c", "", true);
	EXPECT(v.ok && std::find(v.argv.begin(), v.argv.end(), "-vn") != v.argv.end() && v.argv[0] == "ffmpeg");
	EXPECT(!plan(root / "c" / "movie.mp4", "mp3", root / "c", "", false).ok);
	return 0;
}

static int testWindow(const fs::path& root) {
	write(root / "w" / "a.png", "1");
	write(root / "w" / "b.png", "22");
	write(root / "w" / "c.txt", "333");
	fs::create_directories(root / "w" / "folder");

	FilesApp app;
	raven::Window window = colibry::window_from_file(std::string(COLIBRY_SOURCE_DIR) + "/app/files.xml");
	EXPECT(window.is_valid());
	app.attach(window, root / "w");
	EXPECT(app.directory() == fs::weakly_canonical(root / "w"));
	EXPECT(app.entries().size() == 4 && app.entries().front().isDir);  // folder first

	EXPECT(app.selectByPattern("*.png") == 2 && app.selection().count() == 2);
	app.invertSelection();
	EXPECT(app.selection().count() == 2);  // folder + c.txt
	app.selectRow(1);
	app.toggleRow(2);
	EXPECT(app.selection().count() == 2);
	app.selectAll();
	app.copySelection(false);
	app.newFolder("made");
	EXPECT(fs::is_directory(root / "w" / "made"));

	// copy a.png into "made": select it, copy, open the folder, paste
	app.refresh();
	int row = -1;
	for (std::size_t i = 0; i < app.entries().size(); ++i) if (app.entries()[i].name == "a.png") row = static_cast<int>(i);
	EXPECT(row >= 0);
	app.selectRow(row);
	app.copySelection(false);
	for (std::size_t i = 0; i < app.entries().size(); ++i) if (app.entries()[i].name == "made") app.activate(static_cast<int>(i));
	EXPECT(app.directory().filename() == "made");
	app.paste();
	EXPECT(fs::exists(root / "w" / "made" / "a.png"));
	app.back();
	EXPECT(app.directory().filename() == "w");
	app.forward();
	EXPECT(app.directory().filename() == "made");
	app.back();
	app.up();  // a new visit: the forward part of the history is dropped
	EXPECT(app.directory().filename() == "win");
	app.forward();
	EXPECT(app.directory().filename() == "win");
	app.open(root / "w");

	// deletion needs two presses
	app.selectRow(row);
	app.deleteSelection();
	EXPECT(app.deletionArmed() && fs::exists(root / "w" / "a.png"));
	app.deleteSelection();
	EXPECT(!fs::exists(root / "w" / "a.png"));

	app.selectRow(static_cast<int>(app.entries().size()) - 1);
	app.showProperties();
	EXPECT(app.propertiesText().find("Name:") != std::string::npos);
	app.convertSelection("png");  // not convertible: reported, nothing happens
	EXPECT(!app.status().empty());
	return 0;
}

int main() {
	TempDir tmp("files");
	RUN(testSelection());
	RUN(testNavigation());
	RUN(testFileOps(tmp.path / "ops"));
	RUN(testProperties(tmp.path / "props"));
	RUN(testArchive(tmp.path / "arc"));
	RUN(testConvert(tmp.path / "conv"));
	EXPECT(colibry::init({"files-test", true}));
	RUN(testWindow(tmp.path / "win"));
	raven::shutdown();
	std::printf("files: all checks passed\n");
	return 0;
}
