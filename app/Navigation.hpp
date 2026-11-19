// Back / forward history of visited folders (MIT).
#pragma once

#include <filesystem>
#include <vector>
#include <utility>

namespace colibry::files {

class Navigation {
public:
	void visit(const std::filesystem::path& path) {
		if (m_index >= 0 && m_entries[static_cast<std::size_t>(m_index)] == path) return;
		m_entries.resize(static_cast<std::size_t>(m_index + 1));  // a new visit drops the forward part
		m_entries.push_back(path);
		if (m_entries.size() > kMaxEntries) m_entries.erase(m_entries.begin());
		m_index = static_cast<int>(m_entries.size()) - 1;
	}
	bool canGoBack() const { return m_index > 0; }
	bool canGoForward() const { return m_index >= 0 && m_index + 1 < static_cast<int>(m_entries.size()); }
	std::filesystem::path back() { return canGoBack() ? m_entries[static_cast<std::size_t>(--m_index)] : current(); }
	std::filesystem::path forward() { return canGoForward() ? m_entries[static_cast<std::size_t>(++m_index)] : current(); }
	std::filesystem::path current() const { return m_index >= 0 ? m_entries[static_cast<std::size_t>(m_index)] : std::filesystem::path(); }

private:
	static constexpr std::size_t kMaxEntries = 200;
	std::vector<std::filesystem::path> m_entries;
	int m_index = -1;
};

// Copy / cut clipboard of the file manager (kept inside the application).
struct Clipboard {
	std::vector<std::filesystem::path> paths;
	bool cut = false;  // true: paste moves the items, false: paste copies them

	void set(std::vector<std::filesystem::path> p, bool isCut) { paths = std::move(p); cut = isCut; }
	bool hasFiles() const { return !paths.empty(); }
	void clear() { paths.clear(); cut = false; }
};

}  // namespace colibry::files
