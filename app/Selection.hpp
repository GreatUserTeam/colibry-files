// `fselect`: the set of selected rows of a file list (MIT).
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace colibry::files {

class Selection {
public:
	// Starts again with `count` rows, none selected.
	void reset(std::size_t count) { m_on.assign(count, false); m_anchor = npos; }
	std::size_t rows() const { return m_on.size(); }
	std::size_t count() const;
	bool isSelected(std::size_t row) const { return row < m_on.size() && m_on[row]; }
	std::vector<std::size_t> indices() const;

	void clear() { m_on.assign(m_on.size(), false); }
	void selectOnly(std::size_t row);
	void toggle(std::size_t row);                 // Ctrl+click: also moves the anchor
	void selectRange(std::size_t row);            // Shift+click: anchor..row replaces the selection
	void selectAll() { m_on.assign(m_on.size(), true); }
	void invert();
	// Selects the names matching a pattern with * ? [abc] [a-z] [!x] (case-insensitive).
	// `add` keeps the current selection. Returns how many rows matched.
	std::size_t selectGlob(const std::vector<std::string>& names, const std::string& pattern, bool add = false);

	static bool globMatch(const std::string& pattern, const std::string& text);

private:
	static constexpr std::size_t npos = static_cast<std::size_t>(-1);
	std::vector<bool> m_on;
	std::size_t m_anchor = npos;
};

}  // namespace colibry::files
