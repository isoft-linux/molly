/*
 * Copyright (C) 2017 Leslie Zhai <xiang.zhai@i-soft.com.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "partrestorewidget.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QProgressBar>

#include <UDisks2Qt5/UDisksDrive>
#include <UDisks2Qt5/UDisksPartition>
#include <UDisks2Qt5/UDisksBlock>
#include <UDisks2Qt5/UDisksFilesystem>

#include <libpartclone.h>
#include <pthread.h>

static QProgressBar *m_progress = Q_NULLPTR;
static pthread_t m_thread;
static pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *callBack(void *percent, void *remaining);

PartRestoreWidget::PartRestoreWidget(UDisksClient *oUDisksClient, 
                                     QWidget *parent, 
                                     Qt::WindowFlags f)
  : QWidget(parent, f), 
    m_UDisksClient(oUDisksClient)
{
    connect(m_UDisksClient, &UDisksClient::objectAdded, [=](const UDisksObject::Ptr &object) {
        getDriveObjects();
    });
    connect(m_UDisksClient, &UDisksClient::objectRemoved, [=](const UDisksObject::Ptr &object) {
        getDriveObjects();
    });
    connect(m_UDisksClient, &UDisksClient::objectsAvailable, [=]() {
        getDriveObjects();
    });

    QVBoxLayout *vbox = new QVBoxLayout;
    auto *label = new QLabel("Restore Partition:");
    vbox->addWidget(label);
    m_combo = new QComboBox;
    vbox->addWidget(m_combo);
    m_table = new QTableWidget;
    QStringList headers {tr("Partition"), tr("Size"), tr("Type"), tr("OS"), tr("Status")};
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    auto *confirmBtn = new QPushButton(tr("Confirm"));
    confirmBtn->setEnabled(false);
    connect(confirmBtn, &QPushButton::clicked, [=]() {
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        if (!m_imgPath.isEmpty() && items.size()) {
            pthread_create(&m_thread, NULL, startRoutine, this);
        }
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, [=]() {
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        if (items.size()) {
            confirmBtn->setEnabled(true);
        }
    });
    vbox->addWidget(m_table);
    m_progress = new QProgressBar;
    m_progress->setTextVisible(true);
    vbox->addWidget(m_progress);
    m_progress->setVisible(false);
    auto *hbox = new QHBoxLayout;
    vbox->addLayout(hbox);
    hbox->addWidget(confirmBtn);
    auto *backBtn = new QPushButton(tr("Back"));
    connect(backBtn, &QPushButton::clicked, [=]() { Q_EMIT back(); });
    hbox->addWidget(backBtn);
    setLayout(vbox);

    connect(this, &PartRestoreWidget::error, [=](QString message) {
        m_progress->setValue(0);
        m_progress->setVisible(false);
    });
    connect(this, &PartRestoreWidget::finished, [=]() {
        m_progress->setValue(0);
        m_progress->setVisible(false);
    });
}

PartRestoreWidget::~PartRestoreWidget()
{
}

bool PartRestoreWidget::isPartAbleToShow(const UDisksPartition *part, 
                                         UDisksBlock *blk,
                                         UDisksFilesystem *fsys, 
                                         QTableWidgetItem *item) 
{
#ifdef DEBUG
    if (fsys)
        qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << fsys->mountPoints();
#endif
    if (!part || part->size() < m_imgUsed || part->isContainer() || !blk || 
        blk->idType() != m_imgType || !fsys || 
        (!fsys->mountPoints().isEmpty() && !fsys->mountPoints().contains("/var/lib/os-prober/mount"))) {
        item->setFlags(Qt::NoItemFlags);
        return false;
    }

    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    return true;
}

void PartRestoreWidget::comboTextChanged(QString text) 
{
    m_table->setRowCount(0);
    m_table->clearContents();

    if (text.isEmpty())
        return;

    QDBusObjectPath tblPath = QDBusObjectPath(udisksDBusPathPrefix + text.mid(5));
    QList<UDisksPartition *> parts;
    for (const UDisksObject::Ptr partPtr : m_UDisksClient->getPartitions(tblPath)) {
        UDisksPartition *part = partPtr->partition();
        parts << part;
    }
    qSort(parts.begin(), parts.end(), [](UDisksPartition *p1, UDisksPartition *p2) -> bool {
        return p1->number() < p2->number();
    });

    int row = 0;
    for (const UDisksPartition *part : parts) {
        UDisksObject::Ptr objPtr = m_UDisksClient->getObject(QDBusObjectPath(udisksDBusPathPrefix + text.mid(5) + QString::number(part->number())));
        if (!objPtr)
            continue;
        UDisksBlock *blk = objPtr->block();
        if (!blk) 
            continue;
        UDisksFilesystem *fsys = objPtr->filesystem();

        m_table->insertRow(row);
        QString partStr = text + QString::number(part->number());
        auto *item = new QTableWidgetItem(partStr);
        isPartAbleToShow(part, blk, fsys, item);
        m_table->setItem(row, 0, item);

        item = new QTableWidgetItem(QString::number(part->size() / 1073741824.0, 'f', 1) + " G");
        isPartAbleToShow(part, blk, fsys, item);
        m_table->setItem(row, 1, item);

        item = new QTableWidgetItem(blk->idType());
        isPartAbleToShow(part, blk, fsys, item);
        m_table->setItem(row, 2, item);

#ifdef DEBUG
        qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << m_OSMap << partStr;
#endif
        item = new QTableWidgetItem(m_OSMap[partStr]);
        isPartAbleToShow(part, blk, fsys, item);
        m_table->setItem(row, 3, item);

        item = new QTableWidgetItem("");
        isPartAbleToShow(part, blk, fsys, item);
        m_table->setItem(row, 4, item);
        row++;
    }

    UDisksObject::Ptr blkPtr = m_UDisksClient->getObject(tblPath);
    if (blkPtr) {
        UDisksBlock *blk = blkPtr->block();
        if (blk) {
            UDisksObject::Ptr drvPtr = blk->driveObjectPtr();
            if (drvPtr) {
                UDisksDrive *drv = drvPtr->drive();
                if (drv)
                    m_combo->setToolTip(drv->id());
            }
        }
    }
}

void PartRestoreWidget::getDriveObjects() 
{
    m_combo->clear();

    for (const UDisksObject::Ptr drvPtr : m_UDisksClient->getObjects(UDisksObject::Drive)) {
        UDisksDrive *drv = drvPtr->drive();
        if (!drv)
            continue;
        UDisksBlock *blk = drv->getBlock();
        if (!blk)
            continue;
        m_combo->addItem(blk->preferredDevice());
    }

    connect(m_combo, &QComboBox::currentTextChanged, [=](const QString &text) {
        comboTextChanged(text);
    });
    comboTextChanged(m_combo->currentText());
}

void PartRestoreWidget::setOSMap(OSMapType OSMap) 
{
    m_OSMap = OSMap;
    comboTextChanged(m_combo->currentText());
}

void PartRestoreWidget::setImgPath(QString imgPath) 
{
    m_imgPath = imgPath;
    if (m_imgPath.isEmpty())
        return;
    partInfo_t info;
    partInfo(m_imgPath.toStdString().c_str(), &info);
    m_imgType = QString(info.type);
    // Convert to udisks
    if (m_imgType == "EXTFS")
        m_imgType = "ext";
    else if (m_imgType == "NTFS")
        m_imgType = "ntfs";
    else if (m_imgType.startsWith("FAT"))
        m_imgType = "vfat";
    m_imgUsed = info.used;
#ifdef DEBUG
    qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << m_imgPath << m_imgType << m_imgUsed;
#endif
    comboTextChanged(m_combo->currentText());
}

static void *callBack(void *percent, void *remaining) 
{
    pthread_mutex_trylock(&m_mutex);
    float *value = (float *)percent;
    char *str = (char *)remaining;
    if (m_progress) {
        m_progress->setValue((int)*value);
        m_progress->setFormat(QString::number((int)*value) + "% " + QString(str));
    }
    pthread_mutex_unlock(&m_mutex);
    return Q_NULLPTR;
}

void *PartRestoreWidget::errorRoutine(void *arg, void *msg) 
{
    PartRestoreWidget *thisPtr = (PartRestoreWidget *)arg;
    if (!thisPtr)
        return Q_NULLPTR;

    char *str = (char *)msg;
    Q_EMIT thisPtr->error(str ? QString(str) : "");

    return Q_NULLPTR;
}

void *PartRestoreWidget::startRoutine(void *arg) 
{
    PartRestoreWidget *thisPtr = (PartRestoreWidget *)arg;
    if (!thisPtr || !thisPtr->m_table || !thisPtr->m_UDisksClient || 
        thisPtr->m_imgPath.isEmpty()) {
        return Q_NULLPTR;
    }
    partType type = LIBPARTCLONE_UNKNOWN;
    QList<QTableWidgetItem *> items = thisPtr->m_table->selectedItems();
    if (items.isEmpty())
        return Q_NULLPTR;
    QString part = items[0]->text();

    if (part.isEmpty())
        return Q_NULLPTR;
    UDisksObject::Ptr blkPtr = thisPtr->m_UDisksClient->getObject(QDBusObjectPath(udisksDBusPathPrefix + part.mid(5)));
    if (!blkPtr)
        return Q_NULLPTR;
    UDisksBlock *blk = blkPtr->block();
    if (!blk)
        return Q_NULLPTR;
    QString strType = blk->idType();
    if (strType.startsWith("ext"))
        type = LIBPARTCLONE_EXTFS;
    else if (strType == "ntfs")
        type = LIBPARTCLONE_NTFS;
    else if (strType == "vfat")
        type = LIBPARTCLONE_FAT;
    // TODO: more file system test
    partRestore(type, 
                thisPtr->m_imgPath.toStdString().c_str(), 
                part.toStdString().c_str(),
                callBack,
                errorRoutine,
                thisPtr);
    Q_EMIT thisPtr->finished();
}

#include "moc_partrestorewidget.cpp"
