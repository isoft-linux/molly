/*
 * Copyright (C) 2017 Leslie Zhai <xiang.zhai@i-soft.com.cn>
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

#include "disktodiskwidget.h"


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

DiskToDiskWidget::DiskToDiskWidget(OSMapType OSMap,
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
    label = new QLabel(tr("Will copy to:"));
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
            label->setText(items[1]->text() + " " + tr("Will copy to:"));
            char src[32]="";
            snprintf(src,sizeof(src),"%s",qPrintable(items[3]->text()));
            uint64_t result = 0ULL;
            get_size(src,&result);
            int rows = m_toTable->rowCount();
            int columns = m_toTable->columnCount();
            for (int i = 0; i<rows;++i){
                QTableWidgetItem *cell= m_toTable->item(i,3);
                if (cell) {
                    char src2[32]="";
                    snprintf(src2,sizeof(src2),"%s",qPrintable(cell->text()));
                    uint64_t result2 = 0ULL;
                    get_size(src2,&result2);
                    for (int j = 0;j<columns;++j){
                        cell = m_toTable->item(i,6); // showflag[1] or [0]
                        if (cell) {
                            if (cell->text() == "1") {
                                cell = m_toTable->item(i,j);
                                if (cell) {
                                  cell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                                }
                            }
                        }
                    }
                    for (int j = 0;j<columns;++j){
                        cell= m_toTable->item(i,j);
                        if (cell) {
                            if (result > result2 && cell->flags() !=Qt::NoItemFlags) {
                              cell->setFlags(Qt::NoItemFlags);
                            }
                        }
                    }
                }
            }
        }
    });
    vbox->addWidget(m_table);

    vbox->addWidget(label);

    m_toTable = new QTableWidget;
    m_toTable->setColumnCount(headers.size()+1);
    m_toTable->hideColumn(headers.size());
    m_toTable->setHorizontalHeaderLabels(headers);
    m_toTable->verticalHeader()->setVisible(false);
    m_toTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_toTable->setSelectionMode(QAbstractItemView::SingleSelection);
    vbox->addWidget(m_toTable);
    hbox = new QHBoxLayout;
    vbox->addLayout(hbox);
    hbox = new QHBoxLayout;
    m_progressd2f = new QProgressBar;
    vbox->addWidget(m_progressd2f);

    m_progressd2f->setVisible(false);
    connect(m_cloneBtn, &QPushButton::clicked, [=]() {
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        QList<QTableWidgetItem *> toItems = m_toTable->selectedItems();
        if (items.size() && toItems.size()) {
            m_progressd2f->setVisible(true);
            m_progressd2f->setValue(0);
            m_cloneBtn->setEnabled(false);
            m_srcDisk = items[1]->text();
            m_dstDisk = toItems[1]->text(); // /dev/sda
            m_timer->stop();
            printf("\n%d,from[%s],to[%s]\n",__LINE__,qPrintable(items[1]->text()),qPrintable(toItems[1]->text()));

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

    connect(this, &DiskToDiskWidget::error, [=](QString message) {
        m_isError = true;
#ifdef DEBUG
        qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << m_isError << message;
#endif
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        if (items.size() > 4) {
            items[5]->setText(tr("Error"));
        }
        m_progressd2f->setVisible(false);
        m_cloneBtn->setEnabled(true);
        m_table->setEnabled(true);
        backBtn->setEnabled(true);
    });
    connect(this, &DiskToDiskWidget::finished, [=]() {
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

DiskToDiskWidget::~DiskToDiskWidget()
{

}

void DiskToDiskWidget::getDriveObjects()

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

    int rows = m_table->rowCount();
    int columns = m_table->columnCount();
    m_toTable->setColumnCount(columns);
    m_toTable->setRowCount(rows);
    for (int i = 0; i<columns;++i){
        for (int j = 0;j<rows;++j){
          QTableWidgetItem *cell= m_table->item(j,i);
          QTableWidgetItem *cell2=new QTableWidgetItem(*cell);
          m_toTable->setItem(j,i,cell2);
        }
    }
}

bool DiskToDiskWidget::isDiskAbleToShow(bool setFlag,
                                        QTableWidgetItem *item)
{
    if (setFlag) {
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        return true;
    }
    item->setFlags(Qt::NoItemFlags);
    return false;
}
void DiskToDiskWidget::advanceProgressBar()
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

void *DiskToDiskWidget::errorRoutine(void *arg, void *msg)
{
    DiskToDiskWidget *thisPtr = (DiskToDiskWidget *)arg;
    if (!thisPtr)
        return Q_NULLPTR;

    char *str = (char *)msg;
    Q_EMIT thisPtr->error(str ? QString(str) : "");

    return Q_NULLPTR;
}

void *DiskToDiskWidget::startRoutined2d(void *arg)
{
    DiskToDiskWidget *thisPtr = (DiskToDiskWidget *)arg;
    QString srcDisk = m_srcDisk;
    QString dstPath = m_dstDisk;
    QString cmd;

    if (m_dstDisk.isEmpty() || m_srcDisk.isEmpty())
        goto cleanup;

    /*
    $ dd if=/dev/sda of=/dev/sdb bs=4096
    */

    cmd = "/usr/bin/dd if=" + srcDisk + " of=" + dstPath + " bs=4096 ";

    printf("%d,will use dd to clone[%s]to[%s],dd cmd[%s]\n",
           __LINE__,
           qPrintable(srcDisk),qPrintable(dstPath),
           qPrintable(cmd));

    g_progressValue = 1;
    system(qPrintable(cmd));
    g_progressValue = 0;

cleanup:
    Q_EMIT thisPtr->finished();

    pthread_detach(pthread_self());
    return Q_NULLPTR;
}


#include "moc_disktodiskwidget.cpp"
