#include "Selection.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace colibry::files {
namespace {

char fold(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

bool match(const char* p, const char* s) {
	while (*p) {
		if (*p == '*') {
			while (*p == '*') ++p;
			if (!*p) return true;
			for (; *s; ++s)
				if (match(p, s)) return true;
			return match(p, s);
		}
		if (!*s) return false;
		if (*p == '?') {
			++p; ++s;
		} else if (*p == '[') {
			const char* q = p + 1;
			bool negate = (*q == '!' || *q == '^');
			if (negate) ++q;
			bool found = false, closed = false, first = true;
			while (*q && (first || *q != ']')) {
				first = false;
				char lo = fold(*q), hi = lo;
				if (q[1] == '-' && q[2] && q[2] != ']') { hi = fold(q[2]); q += 2; }
				if (fold(*s) >= lo && fold(*s) <= hi) found = true;
				++q;
			}
			closed = (*q == ']');
			if (!closed) {  // no closing bracket: '[' is an ordinary character
				if (fold(*s) != '[') return false;
				++p; ++s;
			} else {
				if (found == negate) return false;
				p = q + 1; ++s;
			}
		} else {
			if (fold(*p) != fold(*s)) return false;
			++p; ++s;
		}
	}
	return !*s;
}

}  // namespace

bool Selection::globMatch(const std::string& pattern, const std::string& text) {
	return match(pattern.c_str(), text.c_str());
}

std::size_t Selection::count() const {
	return static_cast<std::size_t>(std::count(m_on.begin(), m_on.end(), true));
}

std::vector<std::size_t> Selection::indices() const {
	std::vector<std::size_t> out;
	for (std::size_t i = 0; i < m_on.size(); ++i)
		if (m_on[i]) out.push_back(i);
	return out;
}

void Selection::selectOnly(std::size_t row) {
	clear();
	if (row < m_on.size()) { m_on[row] = true; m_anchor = row; }
}

void Selection::toggle(std::size_t row) {
	if (row >= m_on.size()) return;
	m_on[row] = !m_on[row];
	m_anchor = row;
}

void Selection::selectRange(std::size_t row) {
	if (row >= m_on.size()) return;
	if (m_anchor == npos || m_anchor >= m_on.size()) { selectOnly(row); return; }
	const std::size_t a = std::min(m_anchor, row), b = std::max(m_anchor, row);
	const std::size_t anchor = m_anchor;
	clear();
	for (std::size_t i = a; i <= b; ++i) m_on[i] = true;
	m_anchor = anchor;
}

void Selection::invert() {
	for (std::size_t i = 0; i < m_on.size(); ++i) m_on[i] = !m_on[i];
}

std::size_t Selection::selectGlob(const std::vector<std::string>& names, const std::string& pattern, bool add) {
	if (!add) clear();
	std::size_t matched = 0;
	for (std::size_t i = 0; i < names.size() && i < m_on.size(); ++i) {
		if (globMatch(pattern, names[i])) { m_on[i] = true; ++matched; }
	}
	return matched;
}

}  // namespace colibry::files
