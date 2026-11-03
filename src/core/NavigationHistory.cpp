#include "NavigationHistory.h"

namespace colibry::fm {

void NavigationHistory::visit(const QString &path)
{
    if (path == current())
        return;

    while (m_entries.size() > m_index + 1)
        m_entries.removeLast();
    m_entries.append(path);

    if (m_entries.size() > kMaxEntries)
        m_entries.removeFirst();
    m_index = static_cast<int>(m_entries.size()) - 1;
}

QString NavigationHistory::back()
{
    if (!canGoBack())
        return {};
    return m_entries.at(--m_index);
}

QString NavigationHistory::forward()
{
    if (!canGoForward())
        return {};
    return m_entries.at(++m_index);
}

} // namespace colibry::fm
