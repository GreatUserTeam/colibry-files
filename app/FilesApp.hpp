// The file manager window logic on Raven (MIT). Keep the object alive while the window is open.
#pragma once

#include "Archive.hpp"
#include "Convert.hpp"
#include "FileOps.hpp"
#include "Navigation.hpp"
#include "Properties.hpp"
#include "Selection.hpp"

#include <colibry/app.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace colibry::files {

class FilesApp {
public:
	FilesApp() = default;

	// Connects the widgets of files.xml (path, pattern, target, list, status, props) and the files.* actions.
	void attach(raven::Window window, const std::filesystem::path& start);

	// Navigation
	bool open(const std::filesystem::path& dir, bool remember = true);
	void back();
	void forward();
	void up();
	void refresh();
	void activate(int row);  // double click / Enter: enters a folder, or selects a file
	void toggleHidden();

	// fselect
	void selectRow(int row);       // plain click
	void toggleRow(int row);       // Ctrl+click
	void selectTo(int row);        // Shift+click
	void selectAll();
	void invertSelection();
	std::size_t selectByPattern(const std::string& pattern, bool add = false);

	// Operations on the selection (or on the highlighted row when nothing is selected)
	void newFolder(const std::string& name);
	void copySelection(bool cut);
	void paste();
	void rename(const std::string& newName);
	void deleteSelection();        // asks twice: the first call only arms the deletion
	void showProperties();
	void archiveSelection();
	void extractSelection();
	void convertSelection(const std::string& target);

	const std::filesystem::path& directory() const { return m_dir; }
	const std::vector<Entry>& entries() const { return m_entries; }
	const Selection& selection() const { return m_selection; }
	const std::string& status() const { return m_statusText; }
	const std::string& propertiesText() const { return m_propsText; }
	bool deletionArmed() const { return m_deleteArmed; }

private:
	std::vector<std::filesystem::path> targets() const;
	void rebuildList();
	void setStatus(const std::string& text);
	void syncRaven();

	raven::Window m_window;
	raven::TextInput m_pathInput, m_patternInput, m_targetInput;
	raven::ListView m_list;
	raven::Label m_status, m_props;

	std::filesystem::path m_dir;
	Navigation m_history;
	Clipboard m_clipboard;
	Selection m_selection;
	std::vector<Entry> m_entries;
	int m_highlight = -1;
	bool m_showHidden = false;
	bool m_deleteArmed = false;
	std::string m_statusText, m_propsText;
	bool m_syncing = false;
};

}  // namespace colibry::files
