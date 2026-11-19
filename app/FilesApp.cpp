#include "FilesApp.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace colibry::files {

void FilesApp::attach(raven::Window window, const fs::path& start) {
	m_window = window;
	raven::Widget root(window.get_content());
	m_pathInput = raven::TextInput(root.find("path"));
	m_patternInput = raven::TextInput(root.find("pattern"));
	m_targetInput = raven::TextInput(root.find("target"));
	m_list = raven::ListView(root.find("list"));
	m_status = raven::Label(root.find("status"));
	m_props = raven::Label(root.find("props"));

	m_list.set_selection_changed_callback([this](Rid, int row) { if (!m_syncing) selectRow(row); });
	m_list.set_activated_callback([this](Rid, int row) { activate(row); });
	m_pathInput.set_changed_callback([](Rid, const char*) {});

	auto bind = [this](const char* name, auto fn) {
		raven::action_bind(name, [this, fn](Rid, const char*) { fn(); });
	};
	bind("files.back", [this] { back(); });
	bind("files.forward", [this] { forward(); });
	bind("files.up", [this] { up(); });
	bind("files.refresh", [this] { refresh(); });
	bind("files.go", [this] { open(m_pathInput.get_text()); });
	bind("files.hidden", [this] { toggleHidden(); });
	bind("files.newfolder", [this] { newFolder("New folder"); });
	bind("files.copy", [this] { copySelection(false); });
	bind("files.cut", [this] { copySelection(true); });
	bind("files.paste", [this] { paste(); });
	bind("files.delete", [this] { deleteSelection(); });
	bind("files.props", [this] { showProperties(); });
	bind("files.archive", [this] { archiveSelection(); });
	bind("files.extract", [this] { extractSelection(); });
	bind("files.convert", [this] { convertSelection(m_targetInput.get_text()); });
	bind("files.selectall", [this] { selectAll(); });
	bind("files.invert", [this] { invertSelection(); });
	bind("files.glob", [this] { selectByPattern(m_patternInput.get_text()); });

	open(start);
}

bool FilesApp::open(const fs::path& dir, bool remember) {
	std::error_code ec;
	fs::path target = fs::weakly_canonical(dir, ec);
	if (ec || !fs::is_directory(target, ec)) {
		setStatus("Cannot open " + dir.string());
		return false;
	}
	std::string error;
	auto listing = fileops::list(target, m_showHidden, &error);
	if (!error.empty()) {
		setStatus(error);
		return false;
	}
	m_dir = target;
	m_entries = std::move(listing);
	if (remember) m_history.visit(m_dir);
	m_deleteArmed = false;
	m_highlight = -1;
	m_selection.reset(m_entries.size());
	m_pathInput.set_text(m_dir.string().c_str());
	rebuildList();
	setStatus(std::to_string(m_entries.size()) + (m_entries.size() == 1 ? " item" : " items"));
	return true;
}

void FilesApp::back() { if (m_history.canGoBack()) open(m_history.back(), false); }
void FilesApp::forward() { if (m_history.canGoForward()) open(m_history.forward(), false); }
void FilesApp::up() { if (m_dir.has_parent_path() && m_dir.parent_path() != m_dir) open(m_dir.parent_path()); }
void FilesApp::refresh() { open(m_dir, false); }
void FilesApp::toggleHidden() { m_showHidden = !m_showHidden; refresh(); }

void FilesApp::activate(int row) {
	if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
	const Entry& e = m_entries[static_cast<std::size_t>(row)];
	if (e.isDir) open(e.path);
	else showProperties();
}

void FilesApp::selectRow(int row) {
	if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
	m_highlight = row;
	m_deleteArmed = false;
	m_selection.selectOnly(static_cast<std::size_t>(row));
	syncRaven();
}

void FilesApp::toggleRow(int row) {
	if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
	m_highlight = row;
	m_selection.toggle(static_cast<std::size_t>(row));
	syncRaven();
}

void FilesApp::selectTo(int row) {
	if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
	m_highlight = row;
	m_selection.selectRange(static_cast<std::size_t>(row));
	syncRaven();
}

void FilesApp::selectAll() { m_selection.selectAll(); syncRaven(); }
void FilesApp::invertSelection() { m_selection.invert(); syncRaven(); }

std::size_t FilesApp::selectByPattern(const std::string& pattern, bool add) {
	std::vector<std::string> names;
	for (const auto& e : m_entries) names.push_back(e.name);
	const std::size_t n = m_selection.selectGlob(names, pattern, add);
	syncRaven();
	setStatus(std::to_string(n) + " matching");
	return n;
}

std::vector<fs::path> FilesApp::targets() const {
	std::vector<fs::path> out;
	for (std::size_t i : m_selection.indices()) out.push_back(m_entries[i].path);
	if (out.empty() && m_highlight >= 0 && m_highlight < static_cast<int>(m_entries.size()))
		out.push_back(m_entries[static_cast<std::size_t>(m_highlight)].path);
	return out;
}

void FilesApp::newFolder(const std::string& name) {
	OpResult r = fileops::createFolder(m_dir, name);
	if (!r.ok) { setStatus(r.error); return; }
	refresh();
	setStatus("Created " + r.path.filename().string());
}

void FilesApp::copySelection(bool cut) {
	auto t = targets();
	if (t.empty()) { setStatus("Nothing selected."); return; }
	m_clipboard.set(t, cut);
	setStatus(std::to_string(t.size()) + (cut ? " cut" : " copied"));
}

void FilesApp::paste() {
	if (!m_clipboard.hasFiles()) { setStatus("The clipboard is empty."); return; }
	int done = 0;
	std::string error;
	for (const auto& p : m_clipboard.paths) {
		OpResult r = m_clipboard.cut ? fileops::move(p, m_dir) : fileops::copy(p, m_dir);
		if (r.ok) ++done; else error = r.error;
	}
	if (m_clipboard.cut) m_clipboard.clear();
	refresh();
	setStatus(error.empty() ? std::to_string(done) + " pasted" : error);
}

void FilesApp::rename(const std::string& newName) {
	auto t = targets();
	if (t.size() != 1) { setStatus("Select one item to rename."); return; }
	OpResult r = fileops::rename(t.front(), newName);
	if (!r.ok) { setStatus(r.error); return; }
	refresh();
	setStatus("Renamed");
}

void FilesApp::deleteSelection() {
	auto t = targets();
	if (t.empty()) { setStatus("Nothing selected."); return; }
	if (!m_deleteArmed) {
		m_deleteArmed = true;
		setStatus("Delete " + std::to_string(t.size()) + " item(s) for good? Press Delete again.");
		return;
	}
	std::string error;
	for (const auto& p : t) {
		OpResult r = fileops::remove(p);
		if (!r.ok) error = r.error;
	}
	m_deleteArmed = false;
	refresh();
	setStatus(error.empty() ? "Deleted" : error);
}

void FilesApp::showProperties() {
	auto t = targets();
	if (t.empty()) { m_propsText = "Folder: " + m_dir.string(); }
	else if (t.size() == 1) m_propsText = describe(t.front()).text();
	else {
		std::uintmax_t total = 0;
		for (const auto& p : t) total += fileops::totalSize(p);
		m_propsText = std::to_string(t.size()) + " items, " + fileops::humanSize(total);
	}
	m_props.set_text(m_propsText.c_str());
}

void FilesApp::archiveSelection() {
	ArchiveResult r = archive::create(targets(), m_dir);
	if (!r.ok) { setStatus(r.error); return; }
	refresh();
	setStatus("Created " + r.path.filename().string());
}

void FilesApp::extractSelection() {
	auto t = targets();
	if (t.size() != 1) { setStatus("Select one archive."); return; }
	ArchiveResult r = archive::extract(t.front(), m_dir);
	if (!r.ok) { setStatus(r.error); return; }
	refresh();
	setStatus("Extracted to " + r.path.filename().string());
}

void FilesApp::convertSelection(const std::string& target) {
	auto t = targets();
	if (t.size() != 1) { setStatus("Select one file to convert."); return; }
	ConvertPlan p = convert::run(t.front(), target, m_dir);
	if (!p.ok) { setStatus(p.error); return; }
	refresh();
	setStatus("Created " + p.output.filename().string());
}

void FilesApp::rebuildList() {
	m_syncing = true;
	m_list.clear_items();
	for (const auto& e : m_entries) m_list.add_item(((e.isDir ? "[+] " : "    ") + e.name).c_str());
	m_syncing = false;
	syncRaven();
}

void FilesApp::syncRaven() {
	m_syncing = true;
	for (std::size_t i = 0; i < m_entries.size(); ++i) {
		const Entry& e = m_entries[i];
		const std::string text = std::string(m_selection.isSelected(i) ? "* " : "  ") + (e.isDir ? "[+] " : "    ") + e.name +
		                         (e.isDir ? "" : "   " + fileops::humanSize(e.size));
		m_list.set_item_text(static_cast<int>(i), text.c_str());
	}
	m_syncing = false;
}

void FilesApp::setStatus(const std::string& text) {
	m_statusText = text;
	m_status.set_text(text.c_str());
}

}  // namespace colibry::files
