#pragma once

#include <QString>
#include <QStringList>

namespace colibry::fm {

/// Back / forward list of visited folders, like a browser history.
class NavigationHistory
{
public:
    /// Records a visit. Visiting the current folder again does nothing; visiting a
    /// new one discards the forward list.
    void visit(const QString &path);

    bool canGoBack() const { return m_index > 0; }
    bool canGoForward() const { return m_index >= 0 && m_index < m_entries.size() - 1; }

    /// Moves back and returns the folder to show (empty when there is none).
    QString back();
    QString forward();

    QString current() const { return m_index >= 0 ? m_entries.at(m_index) : QString(); }

private:
    static constexpr int kMaxEntries = 200;

    QStringList m_entries;
    int m_index = -1;
};

} // namespace colibry::fm
