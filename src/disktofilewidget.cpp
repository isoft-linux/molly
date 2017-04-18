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

#include <pthread.h>
#include <libpartclone.h>

#include "disktofilewidget.h"
#include "imgdialog.h"
#include "utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QHeaderView>

#include <UDisks2Qt5/UDisksObject>
#include <UDisks2Qt5/UDisksDrive>

static UDisksClient *m_UDisksClientd2f = Q_NULLPTR;
static QProgressBar *m_progressd2f = Q_NULLPTR;
static pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t m_thread;
static QString m_part = "";
static QString m_img = "";
static int g_progressValue = 0;

DiskToFileWidget::DiskToFileWidget(OSMapType OSMap,
                                   UDisksClient *oUDisksClient,
                                   QWidget *parent,
                                   Qt::WindowFlags f)
    : QWidget(parent, f),
      m_OSMap(OSMap)
{
    m_UDisksClientd2f = oUDisksClient;
    connect(m_UDisksClientd2f, &UDisksClient::objectAdded, [=](const UDisksObject::Ptr &object) {
        getDriveObjects();
    });
    connect(m_UDisksClientd2f, &UDisksClient::objectRemoved, [=](const UDisksObject::Ptr &object) {
        getDriveObjects();
    });
    connect(m_UDisksClientd2f, &UDisksClient::objectsAvailable, [=]() {
        getDriveObjects();
    });

    m_timer = new QTimer();
    connect(m_timer, SIGNAL(timeout()), this, SLOT(advanceProgressBar()));
    auto *vbox = new QVBoxLayout;
    auto *hbox = new QHBoxLayout;
    auto *label = new QLabel(tr("Please choose the Disk:"));
    hbox->addWidget(label);
    vbox->addLayout(hbox);
    m_table = new QTableWidget;
    QStringList headers {tr("Name"), tr("Disk"), tr("Serial"), tr("Size"), tr("UsedSize"), tr("State")};
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_browseBtn = new QPushButton(tr("Browse"));
    m_cloneBtn = new QPushButton(tr("Clone"));
    auto *edit = new QLineEdit;
    connect(edit, &QLineEdit::textChanged, [=](const QString &text) {
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        m_cloneBtn->setEnabled(items.size() && ImgDialog::isPathWritable(text));
    });
    connect(m_browseBtn, &QPushButton::clicked, [=]() {
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        if (items.size()) {
            auto *dlg = new ImgDialog(items[1]->text(),
                                      m_OSMap,
                                      tr("Choose a Partition to save the image"),
                                      this);
            connect(dlg, &ImgDialog::savePathSelected, [=](QString path) {
                // todo: path is /xx/yy/zz/
                printf("\ntrace:%d,will clont[%s] to [%s].\n",__LINE__,qPrintable(items[1]->text()),qPrintable(path));
                dlg->close();
                edit->setText(path);
            });
            dlg->exec();
        }
    });

    m_browseBtn->setEnabled(false);
    connect(m_table, &QTableWidget::itemSelectionChanged, [=]() {
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        if (items.size()) {
            m_browseBtn->setEnabled(true);
            edit->setText("");
        }
    });
    vbox->addWidget(m_table);
    hbox = new QHBoxLayout;
    label = new QLabel(tr("Partition image save path:"));
    hbox->addWidget(label);
    hbox->addWidget(edit);
    hbox->addWidget(m_browseBtn);
    vbox->addLayout(hbox);
    hbox = new QHBoxLayout;
    m_progressd2f = new QProgressBar;
    vbox->addWidget(m_progressd2f);

    m_progressd2f->setVisible(false);
    connect(m_cloneBtn, &QPushButton::clicked, [=]() {
        if (m_isClone) {
            QList<QTableWidgetItem *> items = m_table->selectedItems();
            if (items.size() && ImgDialog::isPathWritable(edit->text())) {
                m_progressd2f->setVisible(true);
                m_progressd2f->setValue(0);
                m_cloneBtn->setText(tr("Cancel"));
                m_part = items[1]->text(); // /dev/sda
                m_img = edit->text(); // path[/home/test/]
                m_timer->stop();

                pthread_create(&m_thread, NULL, startRoutined2f, this);

                m_timer->start(500);
            }
        } else {
            m_progressd2f->setVisible(false);
            m_cloneBtn->setText(tr("Clone"));
            monitor_processes(CANCEL_DD_STR,NULL,NULL);
            partCloneCancel(1);
            m_timer->stop();
        }
        m_isClone = !m_isClone;
    });
    hbox->addWidget(m_cloneBtn);
    auto *backBtn = new QPushButton(tr("Back"));
    connect(backBtn, &QPushButton::clicked, [=]() { Q_EMIT back(); });
    hbox->addWidget(backBtn);
    vbox->addLayout(hbox);
    setLayout(vbox);

    connect(this, &DiskToFileWidget::error, [=](QString message) {
        m_isError = true;
        m_isClone = true;
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        if (items.size() > 4) {
            items[5]->setText(tr("Error"));
        }
        m_progressd2f->setVisible(false);
        m_cloneBtn->setText(tr("Clone"));
        m_table->setEnabled(true);
        m_browseBtn->setEnabled(true);
        edit->setEnabled(true);
        backBtn->setEnabled(true);
    });
    connect(this, &DiskToFileWidget::finished, [=]() {
        m_isClone = true;
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        if (!m_isError && items.size() > 4) {
            items[5]->setText(tr("Finished"));
        }
        m_progressd2f->setVisible(false);
        m_cloneBtn->setText(tr("Clone"));
        m_cloneBtn->setEnabled(true);
        m_table->setEnabled(true);
        m_browseBtn->setEnabled(true);
        edit->setEnabled(true);
        backBtn->setEnabled(true);
    });
}

DiskToFileWidget::~DiskToFileWidget()
{
    if (m_browseBtn) {
        delete m_browseBtn;
        m_browseBtn = Q_NULLPTR;
    }
}

void DiskToFileWidget::getDriveObjects()

{
    m_browseBtn->setEnabled(false);
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

        row++;
    }
}

bool DiskToFileWidget::isDiskAbleToShow(bool setFlag,
                                        QTableWidgetItem *item)
{
    if (setFlag) {
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        return true;
    }
    item->setFlags(Qt::NoItemFlags);
    return false;
}
void DiskToFileWidget::advanceProgressBar()
{
    if (m_progressd2f && g_progressValue > 0) {
        char pos[64] = "";
        char tsize[64] = "";
        int value = monitor_processes("dd",pos,tsize);
        m_progressd2f->setValue(value);
        m_progressd2f->setFormat(QString::number(value) + "% " + QString(pos) + "/" + QString(tsize) );
    }
}

static void *callBackd2f(void *percent, void *remaining)
{
    pthread_mutex_trylock(&m_mutex);
    float *value = (float *)percent;
    char *str = (char *)remaining;
    if (m_progressd2f) {
        m_progressd2f->setValue((int)*value);
        m_progressd2f->setFormat(QString::number((int)*value) + "% " + QString(str));
    }
    pthread_mutex_unlock(&m_mutex);
    return Q_NULLPTR;
}

void *DiskToFileWidget::errorRoutine(void *arg, void *msg)
{
    DiskToFileWidget *thisPtr = (DiskToFileWidget *)arg;
    if (!thisPtr)
        return Q_NULLPTR;

    char *str = (char *)msg;
    Q_EMIT thisPtr->error(str ? QString(str) : "");

    return Q_NULLPTR;
}

void *DiskToFileWidget::startRoutined2f(void *arg)
{
    DiskToFileWidget *thisPtr = (DiskToFileWidget *)arg;
    QString srcDisk = m_part;
    QString dstPath = m_img;
    partType type = LIBPARTCLONE_UNKNOWN;
    QString strType;
    QMap<QString,QString>::iterator it;
    QMap<QString, QString> disksMap;
    QString cmd;
    QDir dir(dstPath);
    int backupOK = -1;
    unsigned long long diskSize = 0ULL;
    int diskNumber = 0;
    QString cfgFile = dstPath + "/" + DISKCFGFILE;

    if (m_part.isEmpty() || m_img.isEmpty())
        goto cleanup;
    if (!m_UDisksClientd2f)
        goto cleanup;

    if (!dir.exists()) {
        bool ok = dir.mkdir(dstPath);
        if (!ok) {
            goto cleanup;
        }
    }
    /*
    $ parted -s /dev/sda -- unit B print > sda.parted.txt
    $ dd if=/dev/sda of=sdx.start.bin count=256 bs=4096 //==>sdx.start.bin=DISKSTARTBIN
    $ ./test-libpartclone-ntfs -d -c -s /dev/sda1 -o /backup/sda1.img
    $ ./test-libpartclone-extfs -d -c -s /dev/sda2 -o /backup/sda2.img
    */

    cmd = "/usr/sbin/parted -s " + srcDisk + " -- unit B print >  " +
            dstPath + "/" + srcDisk.mid(5) + ".parted.txt ";
    system(qPrintable(cmd));
    cmd = "/usr/bin/dd if=" + srcDisk + " of=" +dstPath + "/" +
            DISKSTARTBIN + " count=256 bs=4096 ";
    backupOK = system(qPrintable(cmd));
    if (backupOK != 0) {
        goto cleanup;
    }

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
        if (sdx != srcDisk) {
            continue;
        }
        diskSize = blk->size();
        QDBusObjectPath tblPath = QDBusObjectPath(udisksDBusPathPrefix + sdx.mid(5));
        QList<UDisksPartition *> parts;
        for (const UDisksObject::Ptr partPtr : m_UDisksClientd2f->getPartitions(tblPath)) {
            UDisksPartition *part = partPtr->partition();
            parts << part;
        }
        qSort(parts.begin(), parts.end(), [](UDisksPartition *p1, UDisksPartition *p2) -> bool {
            return p1->number() < p2->number();
        });
        for (const UDisksPartition *part : parts) {
            if (!part) {
                continue;
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

            if (blk2->idType() == "swap"  ||
                !fsys->mountPoints().isEmpty()) {
                //break; // recheck failed!!!
            }
            disksMap.insert(blk2->preferredDevice(),blk2->idType());
        }
    }
    diskNumber = 0;
    for ( it = disksMap.begin(); it != disksMap.end(); ++it ) {
        strType = it.value();
        if (strType.startsWith("ext"))
            type = LIBPARTCLONE_EXTFS;
        else if (strType == "ntfs")
            type = LIBPARTCLONE_NTFS;
        else if (strType == "fat")
            type = LIBPARTCLONE_FAT;
        else if (strType == "vfat")
            type = LIBPARTCLONE_FAT;
        else if (strType == "exfat")
            type = LIBPARTCLONE_EXFAT;
        else if (strType == "minix")
            type = LIBPARTCLONE_MINIX;
        else
            type = LIBPARTCLONE_UNKNOWN;

        printf("%d,strType[%s]\n",__LINE__,qPrintable(strType));
        diskNumber ++;

        if (type != LIBPARTCLONE_UNKNOWN) {
            QString dst = dstPath + "/" + it.key().mid(5) + DISK_CLONE_EXT_NAME;

            printf("%d,will clone[%s]to[%s]\n",
                   __LINE__,
                   qPrintable(it.key()),qPrintable(dst));

            backupOK = partClone(type,
                  (char *)qPrintable(it.key()),
                  (char *)qPrintable(dst),
                  1,
                  callBackd2f,
                  errorRoutine,
                  thisPtr);
            if (backupOK != 0) {
                goto cleanup;
            }
        } else {
            // meet [permission denied]!!!
            QString dst = dstPath + "/" + it.key().mid(5) + DISK_CLONE_EXT_NAME + ".dd";
            cmd = "/usr/bin/dd if=" + it.key() + " of=" + dst + " bs=4096 ";

            printf("%d,will use dd to clone[%s]to[%s],dd cmd[%s]\n",
                   __LINE__,
                   qPrintable(it.key()),qPrintable(dst),
                   qPrintable(cmd));

            g_progressValue = 1;
            backupOK = system(qPrintable(cmd));
            g_progressValue = 0;

            if (backupOK != 0) {
                goto cleanup;
            }
        }
    }
    // can not find any partitions on disk, so, dd all disk.
    if (diskNumber == 0) {
        QString dst = dstPath + "/" + srcDisk.mid(5) + DISK_CLONE_EXT_NAME + ".dd";
        cmd = "/usr/bin/dd if=" + srcDisk + " of=" + dst  +" bs=4096 ";

        printf("%d,will use dd to clone,dd cmd[%s]\n",
               __LINE__,qPrintable(cmd));

        g_progressValue = 1;
        backupOK = system(qPrintable(cmd));
        g_progressValue = 0;

        if (backupOK != 0) {
            goto cleanup;
        }
    }
    disksMap.clear();
    backupOK = 0;

    diskcfginfo_t cfgInfo;
    memset(&cfgInfo,0,sizeof(diskcfginfo_t));
    cfgInfo.diskSize = diskSize;
    cfgInfo.partNumber = diskNumber;
    if (setDiskCfgInfo(qPrintable(cfgFile),&cfgInfo) != 0) {
        printf("%d,disktofile:write file error!!!\n",__LINE__);
    }

cleanup:

    if (backupOK != 0) {
        Q_EMIT thisPtr->error(tr("Disk backup failed!"));
    } else {
        Q_EMIT thisPtr->finished();
    }

    pthread_detach(pthread_self());
    return Q_NULLPTR;
}

#include "moc_disktofilewidget.cpp"
