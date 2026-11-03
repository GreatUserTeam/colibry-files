#include "MainWindow.h"

#include "FileIcons.h"
#include "Launcher.h"
#include "PropertiesDialog.h"
#include "core/FileClipboard.h"
#include "core/FileOperations.h"
#include "colibry/Keys.h"
#include "colibry/SettingsStore.h"

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QCompleter>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QTreeView>

namespace colibry::fm {

namespace {
constexpr int kPathRole = Qt::UserRole + 1;
}

MainWindow::MainWindow(const QString &startPath, SettingsStore &store, QWidget *parent)
    : QMainWindow(parent)
    , m_store(store)
{
    setWindowTitle(tr("Files"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("system-file-manager")));
    resize(1000, 640);
    setMinimumSize(560, 360);

    buildActions();
    buildViews();
    buildSidebar();
    buildToolBar();
    buildMenus();

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_places);
    splitter->addWidget(m_stack);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({190, 800});
    splitter->setChildrenCollapsible(false);
    setCentralWidget(splitter);

    m_status = new QLabel(this);
    statusBar()->addWidget(m_status, 1);

    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, [this] { updateActions(); });

    // Restore the previous state.
    setShowHidden(m_store.value(Keys::FmShowHidden).toBool());
    setIconView(m_store.value(Keys::FmViewMode).toInt() == 1);
    const QByteArray geometry = m_store.value(Keys::FmGeometry).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    if (!navigateTo(startPath))
        navigateTo(QDir::homePath());
    updateActions();
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

void MainWindow::buildActions()
{
    auto make = [this](const QString &text, QKeySequence shortcut = {}) {
        auto *action = new QAction(text, this);
        if (!shortcut.isEmpty())
            action->setShortcut(shortcut);
        return action;
    };

    m_back = make(tr("Back"), QKeySequence::Back);
    m_back->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    m_forward = make(tr("Forward"), QKeySequence::Forward);
    m_forward->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    m_up = make(tr("Up"), QKeySequence(Qt::ALT | Qt::Key_Up));
    m_up->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    m_home = make(tr("Home"), QKeySequence(Qt::ALT | Qt::Key_Home));
    m_home->setIcon(style()->standardIcon(QStyle::SP_DirHomeIcon));
    m_reload = make(tr("Refresh"), QKeySequence::Refresh);
    m_reload->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));

    m_open = make(tr("Open"));
    m_openSystem = make(tr("Open with Default Application"));
    m_cut = make(tr("Cut"), QKeySequence::Cut);
    m_copy = make(tr("Copy"), QKeySequence::Copy);
    m_paste = make(tr("Paste"), QKeySequence::Paste);
    m_rename = make(tr("Rename…"), QKeySequence(Qt::Key_F2));
    m_trash = make(tr("Move to Trash"), QKeySequence::Delete);
    m_delete = make(tr("Delete Permanently…"), QKeySequence(Qt::SHIFT | Qt::Key_Delete));
    m_newFolder = make(tr("New Folder…"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    m_newFile = make(tr("New Empty File…"));
    m_selectAll = make(tr("Select All"), QKeySequence::SelectAll);
    m_properties = make(tr("Properties"), QKeySequence(Qt::ALT | Qt::Key_Return));

    m_hidden = make(tr("Show Hidden Files"), QKeySequence(Qt::CTRL | Qt::Key_H));
    m_hidden->setCheckable(true);
    m_viewDetails = make(tr("Details"));
    m_viewDetails->setCheckable(true);
    m_viewIcons = make(tr("Icons"));
    m_viewIcons->setCheckable(true);
    auto *viewGroup = new QActionGroup(this);
    viewGroup->addAction(m_viewDetails);
    viewGroup->addAction(m_viewIcons);
    m_viewDetails->setChecked(true);

    connect(m_back, &QAction::triggered, this, [this] { goBack(); });
    connect(m_forward, &QAction::triggered, this, [this] { goForward(); });
    connect(m_up, &QAction::triggered, this, [this] { goUp(); });
    connect(m_home, &QAction::triggered, this, [this] { navigateTo(QDir::homePath()); });
    connect(m_reload, &QAction::triggered, this, [this] { refresh(); });

    connect(m_open, &QAction::triggered, this, [this] { openSelection(); });
    connect(m_openSystem, &QAction::triggered, this, [this] { openSelectionWithSystem(); });
    connect(m_cut, &QAction::triggered, this, [this] { cutSelection(); });
    connect(m_copy, &QAction::triggered, this, [this] { copySelection(); });
    connect(m_paste, &QAction::triggered, this, [this] {
        const PasteResult r = pasteHere();
        reportErrors(tr("Paste"), r.errors);
    });
    connect(m_rename, &QAction::triggered, this, [this] { promptRename(); });
    connect(m_trash, &QAction::triggered, this, [this] { promptTrash(); });
    connect(m_delete, &QAction::triggered, this, [this] { promptDelete(); });
    connect(m_newFolder, &QAction::triggered, this, [this] { promptNewFolder(); });
    connect(m_newFile, &QAction::triggered, this, [this] { promptNewFile(); });
    connect(m_selectAll, &QAction::triggered, this, [this] { currentView()->selectAll(); });
    connect(m_properties, &QAction::triggered, this, [this] { showProperties(); });

    connect(m_hidden, &QAction::toggled, this, [this](bool on) { setShowHidden(on); });
    connect(m_viewDetails, &QAction::triggered, this, [this] { setIconView(false); });
    connect(m_viewIcons, &QAction::triggered, this, [this] { setIconView(true); });
}

void MainWindow::buildViews()
{
    m_model = new QFileSystemModel(this);
    m_model->setIconProvider(FileIcons::shared());
    m_model->setReadOnly(false); // needed for drag and drop; renaming goes through FileOperations
    m_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    m_model->setNameFilterDisables(false);

    m_details = new QTreeView(this);
    m_details->setModel(m_model);
    m_details->setRootIsDecorated(false);
    m_details->setUniformRowHeights(true);
    m_details->setAlternatingRowColors(true);
    m_details->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_details->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_details->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_details->setSortingEnabled(true);
    m_details->sortByColumn(0, Qt::AscendingOrder);
    m_details->setDragDropMode(QAbstractItemView::DragDrop);
    m_details->setDefaultDropAction(Qt::MoveAction);
    m_details->setContextMenuPolicy(Qt::CustomContextMenu);
    m_details->header()->setStretchLastSection(false);
    m_details->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < m_model->columnCount(); ++column)
        m_details->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);

    m_icons = new QListView(this);
    m_icons->setModel(m_model);
    m_icons->setSelectionModel(m_details->selectionModel());
    m_icons->setViewMode(QListView::IconMode);
    m_icons->setResizeMode(QListView::Adjust);
    m_icons->setMovement(QListView::Static);
    m_icons->setGridSize(QSize(104, 92));
    m_icons->setIconSize(QSize(48, 48));
    m_icons->setWordWrap(true);
    m_icons->setUniformItemSizes(true);
    m_icons->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_icons->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_icons->setDragDropMode(QAbstractItemView::DragDrop);
    m_icons->setDefaultDropAction(Qt::MoveAction);
    m_icons->setContextMenuPolicy(Qt::CustomContextMenu);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_details);
    m_stack->addWidget(m_icons);

    // Keyboard shortcuts that edit files only work while a view has the focus,
    // so Ctrl+C inside the path bar still copies text.
    const QList<QAction *> viewActions = {m_cut, m_copy, m_paste, m_rename, m_trash, m_delete,
                                          m_newFolder, m_selectAll, m_properties};
    for (QAction *action : viewActions) {
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        m_stack->addAction(action);
    }

    auto *backspace = new QAction(this);
    backspace->setShortcut(QKeySequence(Qt::Key_Backspace));
    backspace->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(backspace, &QAction::triggered, this, [this] { goUp(); });
    m_stack->addAction(backspace);

    connect(m_details, &QTreeView::activated, this, &MainWindow::activateIndex);
    connect(m_icons, &QListView::activated, this, &MainWindow::activateIndex);
    connect(m_details, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) { showContextMenu(m_details, pos); });
    connect(m_icons, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) { showContextMenu(m_icons, pos); });

    connect(m_details->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        updateActions();
        updateStatus();
    });
    connect(m_model, &QFileSystemModel::directoryLoaded, this, [this] {
        updateStatus();
        trySelectPending();
    });
    connect(m_model, &QAbstractItemModel::rowsInserted, this, [this] {
        updateStatus();
        trySelectPending();
    });
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, [this] { updateStatus(); });
}

void MainWindow::buildSidebar()
{
    m_places = new QListWidget(this);
    m_places->setFrameShape(QFrame::NoFrame);
    m_places->setIconSize(QSize(18, 18));
    m_places->setMinimumWidth(130);

    struct Place
    {
        QString name;
        QStandardPaths::StandardLocation location;
        QStyle::StandardPixmap icon;
    };
    const QList<Place> places = {
        {tr("Home"), QStandardPaths::HomeLocation, QStyle::SP_DirHomeIcon},
        {tr("Desktop"), QStandardPaths::DesktopLocation, QStyle::SP_DirIcon},
        {tr("Documents"), QStandardPaths::DocumentsLocation, QStyle::SP_DirIcon},
        {tr("Downloads"), QStandardPaths::DownloadLocation, QStyle::SP_DirIcon},
        {tr("Music"), QStandardPaths::MusicLocation, QStyle::SP_DirIcon},
        {tr("Pictures"), QStandardPaths::PicturesLocation, QStyle::SP_DirIcon},
        {tr("Videos"), QStandardPaths::MoviesLocation, QStyle::SP_DirIcon},
    };

    QStringList seen;
    for (const Place &place : places) {
        const QString path = QStandardPaths::writableLocation(place.location);
        if (path.isEmpty() || !QFileInfo(path).isDir() || seen.contains(path))
            continue;
        seen << path;
        auto *item = new QListWidgetItem(style()->standardIcon(place.icon), place.name, m_places);
        item->setData(kPathRole, path);
    }
    auto *root = new QListWidgetItem(style()->standardIcon(QStyle::SP_DriveHDIcon), tr("Computer"), m_places);
    root->setData(kPathRole, QDir::rootPath());

    connect(m_places, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        navigateTo(item->data(kPathRole).toString());
    });
}

void MainWindow::buildToolBar()
{
    auto *bar = addToolBar(tr("Navigation"));
    bar->setMovable(false);
    bar->setIconSize(QSize(18, 18));
    bar->addAction(m_back);
    bar->addAction(m_forward);
    bar->addAction(m_up);
    bar->addAction(m_reload);

    m_pathEdit = new QLineEdit(bar);
    m_pathEdit->setClearButtonEnabled(false);
    m_pathEdit->setToolTip(tr("Location (Ctrl+L)"));
    m_completionModel = new QFileSystemModel(this);
    m_completionModel->setIconProvider(FileIcons::shared());
    m_completionModel->setFilter(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    m_completionModel->setRootPath(QDir::rootPath());
    auto *completer = new QCompleter(m_completionModel, this);
    completer->setCaseSensitivity(Qt::CaseSensitive);
    m_pathEdit->setCompleter(completer);
    bar->addWidget(m_pathEdit);

    connect(m_pathEdit, &QLineEdit::returnPressed, this, [this] {
        QString text = m_pathEdit->text().trimmed();
        if (text == QLatin1String("~") || text.startsWith(QStringLiteral("~/")))
            text.replace(0, 1, QDir::homePath());
        if (!navigateTo(text)) {
            statusBar()->showMessage(tr("Cannot open \"%1\"").arg(text), 4000);
            m_pathEdit->setText(m_path);
        } else {
            currentView()->setFocus();
        }
    });

    m_filterEdit = new QLineEdit(bar);
    m_filterEdit->setPlaceholderText(tr("Filter (Ctrl+F)"));
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setMaximumWidth(200);
    bar->addWidget(m_filterEdit);
    connect(m_filterEdit, &QLineEdit::textChanged, this, [this](const QString &text) { setFilterText(text); });

    bar->addSeparator();
    bar->addAction(m_viewDetails);
    bar->addAction(m_viewIcons);

    // Window-wide shortcuts for the two text fields.
    auto *focusPath = new QAction(this);
    focusPath->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(focusPath, &QAction::triggered, this, [this] {
        m_pathEdit->setFocus();
        m_pathEdit->selectAll();
    });
    addAction(focusPath);

    auto *focusFilter = new QAction(this);
    focusFilter->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    connect(focusFilter, &QAction::triggered, this, [this] {
        m_filterEdit->setFocus();
        m_filterEdit->selectAll();
    });
    addAction(focusFilter);

    for (QAction *nav : {m_back, m_forward, m_up, m_home, m_reload, m_hidden})
        addAction(nav);
}

void MainWindow::buildMenus()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(m_newFolder);
    file->addAction(m_newFile);
    file->addSeparator();
    file->addAction(m_open);
    file->addAction(m_openSystem);
    file->addAction(m_properties);
    file->addSeparator();
    QAction *quit = file->addAction(tr("Close"));
    quit->setShortcut(QKeySequence::Close);
    connect(quit, &QAction::triggered, this, &QWidget::close);

    QMenu *edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(m_cut);
    edit->addAction(m_copy);
    edit->addAction(m_paste);
    edit->addSeparator();
    edit->addAction(m_selectAll);
    edit->addAction(m_rename);
    edit->addSeparator();
    edit->addAction(m_trash);
    edit->addAction(m_delete);

    QMenu *view = menuBar()->addMenu(tr("&View"));
    view->addAction(m_viewDetails);
    view->addAction(m_viewIcons);
    view->addSeparator();
    view->addAction(m_hidden);
    view->addAction(m_reload);

    QMenu *go = menuBar()->addMenu(tr("&Go"));
    go->addAction(m_back);
    go->addAction(m_forward);
    go->addAction(m_up);
    go->addAction(m_home);
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

QAbstractItemView *MainWindow::currentView() const
{
    return isIconView() ? static_cast<QAbstractItemView *>(m_icons) : static_cast<QAbstractItemView *>(m_details);
}

bool MainWindow::isIconView() const
{
    return m_stack->currentWidget() == m_icons;
}

bool MainWindow::showsHidden() const
{
    return m_model->filter().testFlag(QDir::Hidden);
}

QStringList MainWindow::selectedPaths() const
{
    QStringList paths;
    const QModelIndexList indexes = m_details->selectionModel()->selectedIndexes();
    for (const QModelIndex &index : indexes) {
        if (index.column() == 0)
            paths << m_model->filePath(index);
    }
    return paths;
}

void MainWindow::setShowHidden(bool show)
{
    QDir::Filters filter = QDir::AllEntries | QDir::NoDotAndDotDot;
    if (show)
        filter |= QDir::Hidden;
    m_model->setFilter(filter);

    const QSignalBlocker blocker(m_hidden);
    m_hidden->setChecked(show);
    m_store.setValue(Keys::FmShowHidden, show);
}

void MainWindow::setIconView(bool icons)
{
    m_stack->setCurrentWidget(icons ? static_cast<QWidget *>(m_icons) : static_cast<QWidget *>(m_details));
    const QSignalBlocker b1(m_viewIcons);
    const QSignalBlocker b2(m_viewDetails);
    m_viewIcons->setChecked(icons);
    m_viewDetails->setChecked(!icons);
    m_store.setValue(Keys::FmViewMode, icons ? 1 : 0);
}

void MainWindow::setFilterText(const QString &text)
{
    const QString trimmed = text.trimmed();
    m_model->setNameFilters(trimmed.isEmpty() ? QStringList()
                                              : QStringList{QStringLiteral("*%1*").arg(trimmed)});
    if (m_filterEdit && m_filterEdit->text() != text) {
        const QSignalBlocker blocker(m_filterEdit);
        m_filterEdit->setText(text);
    }
    updateStatus();
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

bool MainWindow::navigateTo(const QString &path)
{
    const QFileInfo info(path);
    if (path.isEmpty() || !info.isDir() || !info.isReadable() || !info.isExecutable())
        return false;

    const QString clean = QDir::cleanPath(info.absoluteFilePath());
    m_history.visit(clean);
    showPath(clean);
    return true;
}

bool MainWindow::goBack()
{
    if (!m_history.canGoBack())
        return false;
    const QString path = m_history.back();
    if (!QFileInfo(path).isDir())
        return false;
    showPath(path);
    return true;
}

bool MainWindow::goForward()
{
    if (!m_history.canGoForward())
        return false;
    const QString path = m_history.forward();
    if (!QFileInfo(path).isDir())
        return false;
    showPath(path);
    return true;
}

bool MainWindow::goUp()
{
    QDir dir(m_path);
    if (!dir.cdUp())
        return false;
    const QString from = m_path;
    const bool ok = navigateTo(dir.absolutePath());
    if (ok)
        selectPath(from); // highlight the folder we just left
    return ok;
}

void MainWindow::showPath(const QString &path)
{
    m_path = path;
    m_pendingSelect.clear();

    // A filter belongs to one folder.
    {
        const QSignalBlocker blocker(m_filterEdit);
        m_filterEdit->clear();
    }
    m_model->setNameFilters({});

    const QModelIndex root = m_model->setRootPath(m_path);
    m_details->setRootIndex(root);
    m_icons->setRootIndex(root);
    m_details->selectionModel()->clearSelection();

    m_pathEdit->setText(m_path);
    const QString name = QFileInfo(m_path).fileName();
    setWindowTitle(tr("%1 — Files").arg(name.isEmpty() ? m_path : name));

    const QSignalBlocker blocker(m_places);
    m_places->clearSelection();
    for (int i = 0; i < m_places->count(); ++i) {
        if (m_places->item(i)->data(kPathRole).toString() == m_path) {
            m_places->setCurrentRow(i);
            break;
        }
    }

    updateActions();
    updateStatus();
}

void MainWindow::refresh()
{
    const QString path = m_path;
    m_model->setRootPath(QString());
    showPath(path);
}

void MainWindow::activateIndex(const QModelIndex &index)
{
    const QString path = m_model->filePath(index);
    if (m_model->isDir(index))
        navigateTo(path);
    else
        Launcher::openFile(path);
}

void MainWindow::selectPath(const QString &path)
{
    m_pendingSelect = QDir::cleanPath(path);
    trySelectPending();
}

void MainWindow::trySelectPending()
{
    if (m_pendingSelect.isEmpty())
        return;

    const QModelIndex index = m_model->index(m_pendingSelect);
    if (!index.isValid() || QDir::cleanPath(m_model->filePath(index.parent())) != m_path)
        return;

    m_details->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_details->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    currentView()->scrollTo(index);
    m_pendingSelect.clear();
}

// ---------------------------------------------------------------------------
// File operations (no dialogs)
// ---------------------------------------------------------------------------

OpResult MainWindow::createFolder(const QString &name)
{
    const OpResult result = FileOperations::createFolder(m_path, name);
    if (result.ok)
        selectPath(result.path);
    return result;
}

OpResult MainWindow::createFile(const QString &name)
{
    const OpResult result = FileOperations::createFile(m_path, name);
    if (result.ok)
        selectPath(result.path);
    return result;
}

OpResult MainWindow::renameSelected(const QString &newName)
{
    const QStringList selection = selectedPaths();
    if (selection.size() != 1)
        return OpResult::failure(tr("Select exactly one item to rename."));

    const OpResult result = FileOperations::rename(selection.first(), newName);
    if (result.ok)
        selectPath(result.path);
    return result;
}

void MainWindow::copySelection()
{
    const QStringList selection = selectedPaths();
    if (selection.isEmpty())
        return;
    FileClipboard::set(selection, false);
    updateActions();
}

void MainWindow::cutSelection()
{
    const QStringList selection = selectedPaths();
    if (selection.isEmpty())
        return;
    FileClipboard::set(selection, true);
    updateActions();
}

PasteResult MainWindow::pasteHere()
{
    PasteResult result;
    const ClipboardContent content = FileClipboard::get();
    QString last;

    for (const QString &source : content.paths) {
        const OpResult r = content.cut ? FileOperations::move(source, m_path) : FileOperations::copy(source, m_path);
        if (r.ok) {
            ++result.done;
            last = r.path;
        } else {
            result.errors << r.error;
        }
    }

    if (content.cut && result.errors.isEmpty())
        FileClipboard::clear();
    if (!last.isEmpty())
        selectPath(last);
    updateActions();
    return result;
}

PasteResult MainWindow::trashSelection()
{
    PasteResult result;
    const QStringList selection = selectedPaths();
    for (const QString &path : selection) {
        const OpResult r = FileOperations::trash(path);
        if (r.ok)
            ++result.done;
        else
            result.errors << r.error;
    }
    return result;
}

PasteResult MainWindow::deleteSelectionPermanently()
{
    PasteResult result;
    const QStringList selection = selectedPaths();
    for (const QString &path : selection) {
        const OpResult r = FileOperations::remove(path);
        if (r.ok)
            ++result.done;
        else
            result.errors << r.error;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Dialog-driven actions
// ---------------------------------------------------------------------------

void MainWindow::reportErrors(const QString &title, const QStringList &errors)
{
    if (!errors.isEmpty())
        QMessageBox::warning(this, title, errors.join(QLatin1Char('\n')));
}

void MainWindow::promptNewFolder()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New Folder"), tr("Folder name:"), QLineEdit::Normal,
                                               FileOperations::uniqueName(m_path, tr("New folder")), &ok);
    if (!ok)
        return;
    const OpResult r = createFolder(name.trimmed());
    if (!r.ok)
        QMessageBox::warning(this, tr("New Folder"), r.error);
}

void MainWindow::promptNewFile()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New Empty File"), tr("File name:"), QLineEdit::Normal,
                                               FileOperations::uniqueName(m_path, tr("New file.txt")), &ok);
    if (!ok)
        return;
    const OpResult r = createFile(name.trimmed());
    if (!r.ok)
        QMessageBox::warning(this, tr("New Empty File"), r.error);
}

void MainWindow::promptRename()
{
    const QStringList selection = selectedPaths();
    if (selection.size() != 1)
        return;

    bool ok = false;
    const QString current = QFileInfo(selection.first()).fileName();
    const QString name = QInputDialog::getText(this, tr("Rename"), tr("New name:"), QLineEdit::Normal, current, &ok);
    if (!ok || name == current)
        return;
    const OpResult r = renameSelected(name.trimmed());
    if (!r.ok)
        QMessageBox::warning(this, tr("Rename"), r.error);
}

void MainWindow::promptTrash()
{
    const PasteResult r = trashSelection();
    reportErrors(tr("Move to Trash"), r.errors);
}

void MainWindow::promptDelete()
{
    const QStringList selection = selectedPaths();
    if (selection.isEmpty())
        return;

    const QString what = selection.size() == 1 ? tr("\"%1\"").arg(QFileInfo(selection.first()).fileName())
                                               : tr("%1 items").arg(selection.size());
    const auto answer = QMessageBox::warning(
        this, tr("Delete Permanently"), tr("Delete %1 permanently? This cannot be undone.").arg(what),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes)
        return;

    const PasteResult r = deleteSelectionPermanently();
    reportErrors(tr("Delete Permanently"), r.errors);
}

void MainWindow::showProperties()
{
    const QStringList selection = selectedPaths();
    if (selection.size() > 1)
        return;
    PropertiesDialog dialog(selection.isEmpty() ? m_path : selection.first(), this);
    dialog.exec();
}

void MainWindow::openSelection()
{
    const QStringList selection = selectedPaths();
    if (selection.size() == 1 && QFileInfo(selection.first()).isDir()) {
        navigateTo(selection.first());
        return;
    }
    for (const QString &path : selection) {
        if (!QFileInfo(path).isDir())
            Launcher::openFile(path);
    }
}

void MainWindow::openSelectionWithSystem()
{
    const QStringList selection = selectedPaths();
    for (const QString &path : selection)
        Launcher::openWithSystem(path);
}

// ---------------------------------------------------------------------------
// Menus and status
// ---------------------------------------------------------------------------

void MainWindow::showContextMenu(QAbstractItemView *view, const QPoint &pos)
{
    const QModelIndex index = view->indexAt(pos);
    if (index.isValid()) {
        if (!view->selectionModel()->isSelected(index))
            view->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    } else {
        view->selectionModel()->clearSelection();
    }
    updateActions();

    QMenu menu(this);
    if (index.isValid()) {
        menu.addAction(m_open);
        menu.addAction(m_openSystem);
        menu.addSeparator();
        menu.addAction(m_cut);
        menu.addAction(m_copy);
        menu.addAction(m_rename);
        menu.addSeparator();
        menu.addAction(m_trash);
        menu.addAction(m_delete);
        menu.addSeparator();
        menu.addAction(m_properties);
    } else {
        menu.addAction(m_newFolder);
        menu.addAction(m_newFile);
        menu.addSeparator();
        menu.addAction(m_paste);
        menu.addSeparator();
        menu.addAction(m_hidden);
        menu.addAction(m_reload);
        menu.addSeparator();
        menu.addAction(m_properties);
    }
    menu.exec(view->viewport()->mapToGlobal(pos));
}

void MainWindow::updateActions()
{
    const int count = static_cast<int>(selectedPaths().size());
    const bool any = count > 0;

    m_back->setEnabled(m_history.canGoBack());
    m_forward->setEnabled(m_history.canGoForward());
    m_up->setEnabled(!QDir(m_path).isRoot());

    m_open->setEnabled(any);
    m_openSystem->setEnabled(any);
    m_cut->setEnabled(any);
    m_copy->setEnabled(any);
    m_trash->setEnabled(any);
    m_delete->setEnabled(any);
    m_rename->setEnabled(count == 1);
    m_properties->setEnabled(count <= 1);
    m_paste->setEnabled(FileClipboard::hasFiles());
}

void MainWindow::updateStatus()
{
    if (!m_status)
        return;

    const QStringList selection = selectedPaths();
    if (selection.isEmpty()) {
        const int items = m_model->rowCount(m_model->index(m_path));
        m_status->setText(tr("%1 items").arg(items));
        return;
    }

    qint64 bytes = 0;
    int folders = 0;
    for (const QString &path : selection) {
        const QFileInfo info(path);
        if (info.isDir())
            ++folders;
        else
            bytes += info.size();
    }
    QString text = tr("%1 selected").arg(selection.size());
    if (bytes > 0)
        text += tr(" · %1").arg(FileOperations::humanSize(bytes));
    if (folders > 0)
        text += tr(" (folder sizes not counted)");
    m_status->setText(text);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_store.setValue(Keys::FmGeometry, saveGeometry());
    QMainWindow::closeEvent(event);
}

} // namespace colibry::fm
