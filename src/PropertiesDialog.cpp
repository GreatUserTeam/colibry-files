#include "PropertiesDialog.h"

#include "core/FileOperations.h"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QMimeDatabase>
#include <QPointer>
#include <QThread>
#include <QVBoxLayout>

namespace colibry::fm {

namespace {

QString permissionString(QFileDevice::Permissions p)
{
    auto bit = [&](QFileDevice::Permission flag, QChar c) { return p.testFlag(flag) ? c : QLatin1Char('-'); };
    QString s;
    s += bit(QFileDevice::ReadOwner, QLatin1Char('r'));
    s += bit(QFileDevice::WriteOwner, QLatin1Char('w'));
    s += bit(QFileDevice::ExeOwner, QLatin1Char('x'));
    s += bit(QFileDevice::ReadGroup, QLatin1Char('r'));
    s += bit(QFileDevice::WriteGroup, QLatin1Char('w'));
    s += bit(QFileDevice::ExeGroup, QLatin1Char('x'));
    s += bit(QFileDevice::ReadOther, QLatin1Char('r'));
    s += bit(QFileDevice::WriteOther, QLatin1Char('w'));
    s += bit(QFileDevice::ExeOther, QLatin1Char('x'));
    return s;
}

QLabel *value(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    return label;
}

} // namespace

PropertiesDialog::PropertiesDialog(const QString &path, QWidget *parent)
    : QDialog(parent)
{
    const QFileInfo info(path);
    setWindowTitle(tr("Properties: %1").arg(info.fileName().isEmpty() ? path : info.fileName()));
    setMinimumWidth(420);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    layout->addLayout(form);

    const QLocale locale;
    const QMimeDatabase mimeDb;

    form->addRow(tr("Name"), value(info.fileName().isEmpty() ? path : info.fileName(), this));
    form->addRow(tr("Location"), value(info.absolutePath(), this));

    QString type;
    if (info.isSymLink())
        type = tr("Link");
    else if (info.isDir())
        type = tr("Folder");
    else
        type = mimeDb.mimeTypeForFile(info).comment();
    form->addRow(tr("Type"), value(type, this));

    if (info.isSymLink())
        form->addRow(tr("Link target"), value(info.symLinkTarget(), this));

    m_size = value(QString(), this);
    form->addRow(tr("Size"), m_size);

    if (info.birthTime().isValid())
        form->addRow(tr("Created"), value(locale.toString(info.birthTime(), QLocale::LongFormat), this));
    form->addRow(tr("Modified"), value(locale.toString(info.lastModified(), QLocale::LongFormat), this));
    form->addRow(tr("Permissions"), value(permissionString(info.permissions()), this));
    if (!info.owner().isEmpty())
        form->addRow(tr("Owner"), value(info.owner(), this));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    startSizeScan(path);
}

PropertiesDialog::~PropertiesDialog()
{
    m_cancel.store(true);
    if (m_thread) {
        m_thread->wait();
        delete m_thread;
    }
}

void PropertiesDialog::startSizeScan(const QString &path)
{
    const QFileInfo info(path);
    auto show = [this](qint64 bytes) {
        m_size->setText(tr("%1 (%2 bytes)")
                            .arg(FileOperations::humanSize(bytes), QLocale().toString(bytes)));
    };

    if (!info.isDir() || info.isSymLink()) {
        show(info.isSymLink() ? 0 : info.size());
        return;
    }

    m_size->setText(tr("Calculating…"));
    QPointer<PropertiesDialog> self(this);
    m_thread = QThread::create([self, path, this] {
        const qint64 bytes = FileOperations::totalSize(path, &m_cancel);
        if (m_cancel.load())
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, bytes] {
                if (!self)
                    return;
                self->m_size->setText(self->tr("%1 (%2 bytes)")
                                          .arg(FileOperations::humanSize(bytes), QLocale().toString(bytes)));
            },
            Qt::QueuedConnection);
    });
    m_thread->start();
}

} // namespace colibry::fm
