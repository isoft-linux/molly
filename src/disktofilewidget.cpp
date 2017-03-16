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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QHeaderView>

#include <UDisks2Qt5/UDisksObject>
#include <UDisks2Qt5/UDisksDrive>

#define  PART_CLONE_EXT_NAME ".part"

static UDisksClient *m_UDisksClient = Q_NULLPTR;
static QProgressBar *m_progress = Q_NULLPTR;
static pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t m_thread;
static QString m_part = "";
static QString m_img = "";

static void *startRoutine(void *arg);
static void *callBack(void *arg, void *remaining);

#define DIM(x) (sizeof(x)/sizeof(*(x)))

static const char     *sizes[]   = { "EB", "PB", "TB", "GB", "MB", "KB", "B" };
static const uint64_t  exbibytes = 1024ULL * 1024ULL * 1024ULL *
        1024ULL * 1024ULL * 1024ULL;

static void format_size(uint64_t size, char *result)
{
    uint64_t multiplier;
    int i;

    multiplier = exbibytes;

    for (i = 0 ; i < DIM(sizes) ; i++, multiplier /= 1024) {
        if (size < multiplier)
            continue;
        if (size % multiplier == 0)
            sprintf(result, "%u %s", size / multiplier, sizes[i]);
        else
            sprintf(result, "%.1f %s", (float) size / multiplier, sizes[i]);
        return;
    }

    strcpy(result, "0");
    return;
}


DiskToFileWidget::DiskToFileWidget(OSProberType *OSProber,
                                   QWidget *parent,
                                   Qt::WindowFlags f)
    : QWidget(parent, f),
      m_OSProber(OSProber)
{
    connect(m_OSProber, &OSProberType::Found,
        [this](const QString &part, const QString &name, const QString &shortname) {
        m_OSMap[part] = name;
    });
    connect(m_OSProber, &OSProberType::Finished, [=]() { getDriveObjects(); });
    //m_OSProber->Probe();

    m_UDisksClient = new UDisksClient;
    m_UDisksClient->init(); // Don't forget this!
    connect(m_UDisksClient, &UDisksClient::objectAdded, [=](const UDisksObject::Ptr &object) {
        getDriveObjects();
    });
    connect(m_UDisksClient, &UDisksClient::objectRemoved, [=](const UDisksObject::Ptr &object) {
        getDriveObjects();
    });
    connect(m_UDisksClient, &UDisksClient::objectsAvailable, [=]() {
        getDriveObjects();
    });

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
    m_progress = new QProgressBar;
    vbox->addWidget(m_progress);

    m_progress->setVisible(false);
    connect(m_cloneBtn, &QPushButton::clicked, [=]() {
        if (m_isClone) {
            QList<QTableWidgetItem *> items = m_table->selectedItems();
            if (items.size() && ImgDialog::isPathWritable(edit->text())) {
                m_progress->setVisible(true);
                m_cloneBtn->setText(tr("Cancel"));
                m_part = items[1]->text(); // /dev/sda
                m_img = edit->text(); // path[/home/test/]

                pthread_create(&m_thread, NULL, startRoutine, NULL);
            }
        } else {
            m_progress->setVisible(false);
            m_cloneBtn->setText(tr("Clone"));
            partCloneCancel(1);
        }
        m_isClone = !m_isClone;
    });
    hbox->addWidget(m_cloneBtn);
    auto *backBtn = new QPushButton(tr("Back"));
    connect(backBtn, &QPushButton::clicked, [=]() { Q_EMIT back(); });
    hbox->addWidget(backBtn);
    vbox->addLayout(hbox);
    setLayout(vbox);
}

DiskToFileWidget::~DiskToFileWidget()
{
    if (m_UDisksClient) {
        delete m_UDisksClient;
        m_UDisksClient = Q_NULLPTR;
    }

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
    for (const UDisksObject::Ptr drvPtr : m_UDisksClient->getObjects(UDisksObject::Drive)) {
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
        for (const UDisksObject::Ptr partPtr : m_UDisksClient->getPartitions(tblPath)) {
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
                    m_UDisksClient->getObject(QDBusObjectPath(udisksDBusPathPrefix +
                                                              sdx.mid(5) + QString::number(part->number())));
            if (!objPtr)
                continue;
            UDisksBlock *blk2 = objPtr->block();
            if (!blk2)
                continue;
            UDisksFilesystem *fsys = objPtr->filesystem();
            if (!fsys)
                continue;

            printf("%d,blk size[%llu]vs blk2 size[%llu],blk name[%s] vs blk2 name[%s]and blk2 idtype[%s]\n",
                   __LINE__,
                   blk->size(), blk2->size(),
                   qPrintable(sdx),
                   qPrintable(blk2->preferredDevice()),
                   qPrintable(blk2->idType()));

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

        item = new QTableWidgetItem(QString::number(123));
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

static void *callBack(void *arg, void *remaining)
{
    pthread_mutex_trylock(&m_mutex);
    float *percent = (float *)arg;
    if (m_progress)
        m_progress->setValue((int)*percent);
    pthread_mutex_unlock(&m_mutex);
    return Q_NULLPTR;
}

static void *startRoutine(void *arg)
{
    if (m_part.isEmpty() || m_img.isEmpty())
        return Q_NULLPTR;
    if (!m_UDisksClient)
        return Q_NULLPTR;

    QString srcDisk = m_part;
    QString dstPath = m_img;
    partType type = LIBPARTCLONE_UNKNOWN;
    QString strType;

    QDir dir(dstPath);
    if (!dir.exists()) {
        bool ok = dir.mkdir(dstPath);
        if (!ok) {
            return Q_NULLPTR;
        }
    }
    /*
    $ parted -s /dev/sda -- unit B print > sda.parted.txt
    $ dd if=/dev/sda of=sda.start.bin count=256 bs=4096
    $ ./test-libpartclone-ntfs -d -c -s /dev/sda1 -o /backup/sda1.img
    $ ./test-libpartclone-extfs -d -c -s /dev/sda2 -o /backup/sda2.img
    */

    QString cmd = "/usr/sbin/parted -s " + srcDisk + " -- unit B print >  " +
            dstPath + "/" + srcDisk.mid(5) + ".parted.txt ";
    system(qPrintable(cmd));
    cmd = "/usr/bin/dd if=" + srcDisk + " of=" +dstPath + "/" +
            srcDisk.mid(5) + ".start.bin" + " count=256 bs=4096 ";
    system(qPrintable(cmd));

    QMap<QString, QString> disksMap;
    for (const UDisksObject::Ptr drvPtr : m_UDisksClient->getObjects(UDisksObject::Drive)) {
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
        QDBusObjectPath tblPath = QDBusObjectPath(udisksDBusPathPrefix + sdx.mid(5));
        QList<UDisksPartition *> parts;
        for (const UDisksObject::Ptr partPtr : m_UDisksClient->getPartitions(tblPath)) {
            UDisksPartition *part = partPtr->partition();
            parts << part;
        }
        qSort(parts.begin(), parts.end(), [](UDisksPartition *p1, UDisksPartition *p2) -> bool {
            return p1->number() < p2->number();
        });
        bool needCloneFlag = true;
        for (const UDisksPartition *part : parts) {
            if (!part) {
                continue;
            }
            UDisksObject::Ptr objPtr =
                    m_UDisksClient->getObject(QDBusObjectPath(udisksDBusPathPrefix +
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
                needCloneFlag = false;
                //break; // recheck failed!!!
            }
            disksMap.insert(blk2->preferredDevice(),blk2->idType());
        }
    }

    QMap<QString,QString>::iterator it;
    for ( it = disksMap.begin(); it != disksMap.end(); ++it ) {
        strType = it.value();
        if (strType.startsWith("ext"))
            type = LIBPARTCLONE_EXTFS;
        else if (strType == "ntfs")
            type = LIBPARTCLONE_NTFS;

        if (type != LIBPARTCLONE_UNKNOWN) {
            QString dst = dstPath + "/" + it.key().mid(5) + PART_CLONE_EXT_NAME;

            printf("%d,will clone[%s]to[%s]\n",
                   __LINE__,
                   qPrintable(it.key()),qPrintable(dst));

            partClone(type,
                  (char *)qPrintable(it.key()),
                  (char *)qPrintable(dst),
                  1,
                  callBack,
                  Q_NULLPTR,
                  Q_NULLPTR);
        } else {
            // todo:use dd for other types.

        }

    }
    disksMap.clear();

    pthread_detach(pthread_self());
}

#include "moc_disktofilewidget.cpp"
