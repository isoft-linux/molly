/*
 * Copyright (C) 2017 fj <fujiang.zhu@i-soft.com.cn>
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

#include "diskrestorewidget.h"


#include <pthread.h>
#include <libpartclone.h>

#include "imgdialog.h"
#include "utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QHeaderView>

#include <UDisks2Qt5/UDisksObject>
#include <UDisks2Qt5/UDisksDrive>

#define  PART_CLONE_EXT_NAME ".part"

static UDisksClient *m_UDisksClientd2f = Q_NULLPTR;
static QProgressBar *m_progressd2f = Q_NULLPTR;
static pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t m_thread;
static QString m_srcDisk = "";
static QString m_dstDisk = "";
static int g_progressValue = 0;

DiskRestoreWidget::DiskRestoreWidget(OSMapType OSMap,
                                   UDisksClient *oUDisksClient,
                                   QWidget *parent,
                                   Qt::WindowFlags f)
    : QWidget(parent, f),
      m_OSMap(OSMap)
{
    m_UDisksClientd2f = oUDisksClient;
    connect(m_UDisksClientd2f, &UDisksClient::objectAdded, [=](const UDisksObject::Ptr &object) {
        getDriveObjects(0ULL);
    });
    connect(m_UDisksClientd2f, &UDisksClient::objectRemoved, [=](const UDisksObject::Ptr &object) {
        getDriveObjects(0ULL);
    });
    connect(m_UDisksClientd2f, &UDisksClient::objectsAvailable, [=]() {
        getDriveObjects(0ULL);
    });

    m_timer = new QTimer();
    connect(m_timer, SIGNAL(timeout()), this, SLOT(advanceProgressBar()));
    auto *vbox = new QVBoxLayout;
    auto *hbox = new QHBoxLayout;
    auto *label = new QLabel(tr("Please choose the Disk:"));
    hbox->addWidget(label);
    vbox->addLayout(hbox);
    m_label = new QLabel(tr("Will copy to:"));
    m_table = new QTableWidget;
    QStringList headers {tr("Name"), tr("Disk"), tr("Serial"), tr("Size"), tr("UsedSize"), tr("State")};
    m_table->setColumnCount(headers.size()+1);
    m_table->hideColumn(headers.size());
    m_table->setHorizontalHeaderLabels(headers);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_cloneBtn = new QPushButton(tr("Clone"));

    connect(m_table, &QTableWidget::itemSelectionChanged, [=]() {
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        if (items.size()) {
            m_label->setText(m_srcDiskPath + " " + tr("Will be restored to:"));
            char src[32]="";
            snprintf(src,sizeof(src),"%s",qPrintable(items[3]->text()));
        }
    });
    vbox->addWidget(m_table);

    vbox->addWidget(m_label);

    hbox = new QHBoxLayout;
    vbox->addLayout(hbox);
    hbox = new QHBoxLayout;
    m_progressd2f = new QProgressBar;
    vbox->addWidget(m_progressd2f);

    m_progressd2f->setVisible(false);
    connect(m_cloneBtn, &QPushButton::clicked, [=]() {
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        if (items.size()) {
            m_progressd2f->setVisible(true);
            m_progressd2f->setValue(0);
            m_cloneBtn->setEnabled(false);
            m_dstDisk = items[1]->text();
            m_timer->stop();
            printf("\n%d,disk restore,src[%s]dst[%s]\n",__LINE__,qPrintable(m_srcDisk),qPrintable(m_dstDisk));

            pthread_create(&m_thread, NULL, startRoutined2d, this);

            m_timer->start(500);
        }
    });
    hbox->addWidget(m_cloneBtn);
    auto *backBtn = new QPushButton(tr("Back"));
    connect(backBtn, &QPushButton::clicked, [=]() { Q_EMIT back(); });
    hbox->addWidget(backBtn);
    vbox->addLayout(hbox);
    setLayout(vbox);

    connect(this, &DiskRestoreWidget::error, [=](QString message) {
        m_isError = true;
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        if (items.size() > 4) {
            items[5]->setText(tr("Error"));
        }
        m_progressd2f->setVisible(false);
        m_cloneBtn->setEnabled(true);
        m_table->setEnabled(true);
        backBtn->setEnabled(true);
    });
    connect(this, &DiskRestoreWidget::finished, [=]() {
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        if (!m_isError && items.size() > 4) {
            items[5]->setText(tr("Finished"));
        }
        m_progressd2f->setVisible(false);
        m_cloneBtn->setEnabled(true);
        m_table->setEnabled(true);
        backBtn->setEnabled(true);
    });
}

DiskRestoreWidget::~DiskRestoreWidget()
{

}

void DiskRestoreWidget::setSrcDiskPath(QString srcDiskPath)
{
    m_srcDiskPath = srcDiskPath;
    if (m_srcDiskPath.isEmpty())
        return;
    comboTextChanged(m_srcDiskPath);

    m_srcDisk = m_srcDiskPath;
    m_label->setText(m_srcDiskPath + " " + tr("Will be restored to:"));
}

void DiskRestoreWidget::comboTextChanged(QString text)
{
    if (text.isEmpty())
        return;

    QString srcDiskPath = text;

    QString cfgPath = srcDiskPath + "/" + DISKCFGFILE;
    QFile   cfgFile(cfgPath);
    if (!cfgFile.exists()) {
        return;
    }
    diskcfginfo_t cfgInfo;
    memset(&cfgInfo,0,sizeof(diskcfginfo_t));
    if (getDiskCfgInfo(qPrintable(cfgPath),&cfgInfo) != 0) {
        //meet error
    }
    getDriveObjects(cfgInfo.diskSize );//todo: * 1024ULL*1024ULL
}

void DiskRestoreWidget::getDriveObjects(unsigned long long srcDiskSize)

{
    m_table->setRowCount(0);
    m_table->clearContents();
    for (const UDisksObject::Ptr drvPtr : m_UDisksClientd2f->getObjects(UDisksObject::Drive)) {
        UDisksDrive *drv = drvPtr->drive();
        if (!drv)
            continue;
        UDisksBlock *blk = drv->getBlock();
        if (!blk)
            continue;
        if (blk->size() < 1ULL)
            continue;

        QString sdx = blk->preferredDevice();

        QDBusObjectPath tblPath = QDBusObjectPath(udisksDBusPathPrefix + sdx.mid(5));
        QList<UDisksPartition *> parts;
        for (const UDisksObject::Ptr partPtr : m_UDisksClientd2f->getPartitions(tblPath)) {
            UDisksPartition *part = partPtr->partition();
            parts << part;
        }
        bool showFlag = true;
        for (const UDisksPartition *part : parts) {
            if (!part) {
                continue;
            }
            if (part->isContainer()) {
                showFlag = false;
                break;
            }

            UDisksObject::Ptr objPtr =
                    m_UDisksClientd2f->getObject(QDBusObjectPath(udisksDBusPathPrefix +
                                                              sdx.mid(5) + QString::number(part->number())));
            if (!objPtr)
                continue;
            UDisksBlock *blk2 = objPtr->block();
            if (!blk2)
                continue;
            UDisksFilesystem *fsys = objPtr->filesystem();
            if (!fsys)
                continue;

            if (srcDiskSize > blk->size()) {
                showFlag = false;
            }
            if (blk2->idType() == "swap"  ||
                !fsys->mountPoints().isEmpty()) {
                showFlag = false;
                break;
            }
        }

        int row = 0;
        m_table->insertRow(row);
        auto *item = new QTableWidgetItem(drv->id());
        isDiskAbleToShow(showFlag,item);
        m_table->setItem(row, 0, item);

        item = new QTableWidgetItem(sdx);
        isDiskAbleToShow(showFlag,item);
        m_table->setItem(row, 1, item);

        item = new QTableWidgetItem(drv->serial());
        isDiskAbleToShow(showFlag,item);
        m_table->setItem(row, 2, item);

        char sSize[32]="";
        format_size(blk->size(),sSize);
        item = new QTableWidgetItem(sSize);
        isDiskAbleToShow(showFlag,item);
        m_table->setItem(row, 3, item);

        item = new QTableWidgetItem(QString::number(0));
        isDiskAbleToShow(showFlag,item);
        m_table->setItem(row, 4, item);

        item = new QTableWidgetItem(QString("not backup"));
        isDiskAbleToShow(showFlag,item);
        m_table->setItem(row, 5, item);

        QString showStr ="";
        if (showFlag) {
            showStr = "1";
        } else
            showStr = "0";
        item = new QTableWidgetItem(showStr);
        isDiskAbleToShow(showFlag,item);
        m_table->setItem(row, 6, item);

        row++;
    }
}

bool DiskRestoreWidget::isDiskAbleToShow(bool setFlag,
                                        QTableWidgetItem *item)
{
    if (setFlag) {
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        return true;
    }
    item->setFlags(Qt::NoItemFlags);
    return false;
}
void DiskRestoreWidget::advanceProgressBar()
{
    if (m_progressd2f && g_progressValue > 0) {
        char pos[64] = "";
        char tsize[64] = "";
        int value = monitor_processes("dd",pos,tsize);
        if (value >=0 && value <= 100) {
            m_progressd2f->setValue(value);
            m_progressd2f->setFormat(QString::number(value) + "% " + QString(pos) + "/" + QString(tsize) );
        }
    }
}

static void *callBackd2f(void *percent, void *remaining)
{
    pthread_mutex_trylock(&m_mutex);
    float *value = (float *)percent;
    char *str = (char *)remaining;
    if (m_progressd2f) {
        int per = (int)*value;
        if (per >=0 && per <= 100) {
            m_progressd2f->setValue(per);
            m_progressd2f->setFormat(QString::number(per) + "% " + QString(str));
        }
    }
    pthread_mutex_unlock(&m_mutex);
    return Q_NULLPTR;
}

void *DiskRestoreWidget::errorRoutine(void *arg, void *msg)
{
    DiskRestoreWidget *thisPtr = (DiskRestoreWidget *)arg;
    if (!thisPtr)
        return Q_NULLPTR;

    char *str = (char *)msg;
    Q_EMIT thisPtr->error(str ? QString(str) : "");

    return Q_NULLPTR;
}

void *DiskRestoreWidget::startRoutined2d(void *arg)
{
    DiskRestoreWidget *thisPtr = (DiskRestoreWidget *)arg;
    QString srcDisk = m_srcDisk;
    QString dstPath = m_dstDisk;
    QString cmd;
    QDir diskDir = QDir(srcDisk);

    if (m_dstDisk.isEmpty() || m_srcDisk.isEmpty())
        goto cleanup;

    diskDir.setFilter(QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    for (QString imgName : diskDir.entryList()) {
        printf("%d,[%s]\n",
               __LINE__,qPrintable(imgName));
        // todo:use api or dd to restore disk part.
    }

    /*
    $ dd if=/dev/sda of=/dev/sdb bs=4096
    */

    cmd = "/usr/bin/dd if=" + srcDisk + " of=" + dstPath + " bs=4096 ";

    printf("%d,will use dd to clone[%s]to[%s],dd cmd[%s]\n",
           __LINE__,
           qPrintable(srcDisk),qPrintable(dstPath),
           qPrintable(cmd));

    g_progressValue = 1;
    //system(qPrintable(cmd));
    g_progressValue = 0;

cleanup:
    Q_EMIT thisPtr->finished();

    pthread_detach(pthread_self());
    return Q_NULLPTR;
}


#include "moc_diskrestorewidget.cpp"
