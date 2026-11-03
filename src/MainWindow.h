#pragma once

#include "FileIcons.h"
#include "core/FileOperations.h"
#include "core/NavigationHistory.h"

#include <QMainWindow>

class QAbstractItemView;
class QAction;
class QCompleter;
class QFileSystemModel;
class QLabel;
class QLineEdit;
class QListView;
class QListWidget;
class QMenu;
class QModelIndex;
class QStackedWidget;
class QToolBar;
class QTreeView;

namespace colibry {
class SettingsStore;
}

namespace colibry::fm {

struct PasteResult
{
    int done = 0;
    QStringList errors;
};

/// The file manager window: places sidebar, path bar, details / icon views, status bar.
///
/// Everything a user can do is also available as a public method that never shows a dialog,
/// so the behavior can be tested without clicking.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(const QString &startPath, SettingsStore &store, QWidget *parent = nullptr);

    QString currentPath() const { return m_path; }
    QStringList selectedPaths() const;
    QFileSystemModel *model() const { return m_model; }
    QAbstractItemView *currentView() const;
    bool isIconView() const;
    bool showsHidden() const;

    /// Shows a folder and records it in the history. False when it cannot be opened.
    bool navigateTo(const QString &path);
    bool goBack();
    bool goForward();
    bool goUp();

    /// Selects the item now, or as soon as the model has loaded it.
    void selectPath(const QString &path);

    OpResult createFolder(const QString &name);
    OpResult createFile(const QString &name);
    OpResult renameSelected(const QString &newName);

    void copySelection();
    void cutSelection();
    PasteResult pasteHere();
    PasteResult trashSelection();
    PasteResult deleteSelectionPermanently();

    void setShowHidden(bool show);
    void setIconView(bool icons);
    void setFilterText(const QString &text);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildActions();
    void buildMenus();
    void buildToolBar();
    void buildViews();
    void buildSidebar();

    void showPath(const QString &path);
    void activateIndex(const QModelIndex &index);
    void showContextMenu(QAbstractItemView *view, const QPoint &pos);
    void updateActions();
    void updateStatus();
    void trySelectPending();
    void reportErrors(const QString &title, const QStringList &errors);

    // Dialog-driven versions of the public operations.
    void promptNewFolder();
    void promptNewFile();
    void promptRename();
    void promptTrash();
    void promptDelete();
    void showProperties();
    void openSelection();
    void openSelectionWithSystem();
    void refresh();

    SettingsStore &m_store;
    QFileSystemModel *m_model = nullptr;
    QFileSystemModel *m_completionModel = nullptr;
    NavigationHistory m_history;
    QString m_path;
    QString m_pendingSelect;

    QListWidget *m_places = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QLineEdit *m_filterEdit = nullptr;
    QStackedWidget *m_stack = nullptr;
    QTreeView *m_details = nullptr;
    QListView *m_icons = nullptr;
    QLabel *m_status = nullptr;

    QAction *m_back = nullptr;
    QAction *m_forward = nullptr;
    QAction *m_up = nullptr;
    QAction *m_home = nullptr;
    QAction *m_reload = nullptr;
    QAction *m_open = nullptr;
    QAction *m_openSystem = nullptr;
    QAction *m_cut = nullptr;
    QAction *m_copy = nullptr;
    QAction *m_paste = nullptr;
    QAction *m_rename = nullptr;
    QAction *m_trash = nullptr;
    QAction *m_delete = nullptr;
    QAction *m_newFolder = nullptr;
    QAction *m_newFile = nullptr;
    QAction *m_selectAll = nullptr;
    QAction *m_properties = nullptr;
    QAction *m_hidden = nullptr;
    QAction *m_viewDetails = nullptr;
    QAction *m_viewIcons = nullptr;
};

} // namespace colibry::fm
