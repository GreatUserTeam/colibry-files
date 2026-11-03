#pragma once

#include <QDialog>

#include <atomic>

class QLabel;
class QThread;

namespace colibry::fm {

/// Shows details of one file or folder. The size of a folder is counted in the background.
class PropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PropertiesDialog(const QString &path, QWidget *parent = nullptr);
    ~PropertiesDialog() override;

private:
    void startSizeScan(const QString &path);

    QLabel *m_size = nullptr;
    QThread *m_thread = nullptr;
    std::atomic_bool m_cancel{false};
};

} // namespace colibry::fm
