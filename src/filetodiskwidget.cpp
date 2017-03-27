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

#include "filetodiskwidget.h"
#include "imgdialog.h"
#include "utils.h"
#include <libpartclone.h>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QDir>
#include <QLineEdit>
#include <QFileDialog>
#include <QTableWidgetItem>
#include <QTreeWidgetItem>
#include <QStandardPaths>

static UDisksClient *m_UDisksClientd2f = Q_NULLPTR;
static QProgressBar *m_progressd2f = Q_NULLPTR;

FileToDiskWidget::FileToDiskWidget(UDisksClient *oUDisksClient,QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f),
    m_UDisksClient(oUDisksClient)
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

    auto *vbox = new QVBoxLayout;
    auto *hbox = new QHBoxLayout;
    auto *label = new QLabel(tr("Please choose the Disk:"));
    hbox->addWidget(label);
    m_combo = new QComboBox;
    hbox->addWidget(m_combo);
    vbox->addLayout(hbox);
    m_treeWidget = new QTreeWidget;

    QStringList headers {tr("Disk"), tr("Size"),tr("State")};
    m_treeWidget->setColumnCount(headers.size());
    m_treeWidget->setHeaderLabels(headers);
    m_treeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_browseBtn = new QPushButton(tr("Browse"));
    m_nextBtn = new QPushButton(tr("Next"));
    m_edit = new QLineEdit;
    connect(m_edit, &QLineEdit::textChanged, [=](const QString &text) {
        QList<QTreeWidgetItem *> items = m_treeWidget->selectedItems();
        m_nextBtn->setEnabled(items.size());
    });
    connect(m_nextBtn, &QPushButton::clicked, [=]() {
        QString srcDiskPath = m_edit->text();
        if (QDir(srcDiskPath).exists()) {
            Q_EMIT next(srcDiskPath);
            srcDiskPath = "";
        }
    });
    connect(m_browseBtn, &QPushButton::clicked, [=]() {
        QString fileName = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
                QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                QFileDialog::DontResolveSymlinks);
        m_edit->setText(fileName);
        m_nextBtn->setEnabled(true);
    });

    m_browseBtn->setEnabled(true);

    connect(m_treeWidget,SIGNAL(itemClicked(QTreeWidgetItem*,int)),
            this,SLOT(setSelectedItem(QTreeWidgetItem*,int)));

    vbox->addWidget(m_treeWidget);
    hbox = new QHBoxLayout;
    hbox->addWidget(m_edit);
    hbox->addWidget(m_browseBtn);
    vbox->addLayout(hbox);
    hbox = new QHBoxLayout;

    hbox->addWidget(m_nextBtn);
    auto *backBtn = new QPushButton(tr("Back"));
    connect(backBtn, &QPushButton::clicked, [=]() { Q_EMIT back(); });
    hbox->addWidget(backBtn);
    vbox->addLayout(hbox);
    setLayout(vbox);

}

FileToDiskWidget::~FileToDiskWidget()
{
}

void FileToDiskWidget::setSelectedItem(QTreeWidgetItem *item,int index)
{
    QTreeWidgetItem *parent = item->parent();
    if(NULL==parent) {
        m_edit->setText(item->text(0));
        printf("\nroot [%d],[%s][%s][%s]\n",index,qPrintable(item->text(0)),qPrintable(item->text(1)),qPrintable(item->text(2)));
        return;
    }
    printf("\n[%d],[%s]\n",index,qPrintable(parent->text(0)));
}

const QString udisksDBusPathPrefix = "/org/freedesktop/UDisks2/block_devices/";
const QString partImgExt = ".part";
const QString osProberMountPoint = "/var/lib/os-prober/mount";
void FileToDiskWidget::comboTextChanged(QString text)
{
    m_treeWidget->clear();
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
        if (!fsys)
            continue;

        for (QString mount : fsys->mountPoints()) {
            if (mount == "/var/lib/os-prober/mount")
                continue;
            QDir imgDir = QDir(mount, "*" + partImgExt, QDir::Name, QDir::Dirs | QDir::NoSymLinks | QDir::Readable);
            if (mount == "/") {
                QStringList locations = QStandardPaths::standardLocations(QStandardPaths::HomeLocation);
                if (locations.size())
                    imgDir.setPath(locations[0]);
            }
            for (QString name : imgDir.entryList()) {
                // todo:imgPath is path...
                QString imgPath = imgDir.path() + "/" + name;
                QString cfgPath = imgPath + "/" + DISKCFGFILE;
                QFile   cfgFile(cfgPath);
                if (!cfgFile.exists()) {
                    continue;
                }
                diskcfginfo_t cfgInfo;
                memset(&cfgInfo,0,sizeof(diskcfginfo_t));
                if (getDiskCfgInfo(qPrintable(cfgPath),&cfgInfo) != 0) {
                    //meet error
                }
                char sSize[32]="";
                format_size(cfgInfo.diskSize,sSize);

                QStringList list;
                QFile rootFile(imgPath);
                list << QString(imgPath) << QString(sSize) << "not backuped";
                QTreeWidgetItem *rootNode = new QTreeWidgetItem(m_treeWidget,list);
                list.clear();

                QDir diskDir = QDir(imgPath);
                diskDir.setFilter(QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot);
                for (QString imgName : diskDir.entryList()) {
                    QTreeWidgetItem *sdx1 = new QTreeWidgetItem(rootNode,QStringList(imgName));
                    sdx1->setFlags(Qt::NoItemFlags);
                    rootNode->addChild(sdx1);
                }
            }
        }
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

void FileToDiskWidget::getDriveObjects()
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
#include "moc_filetodiskwidget.cpp"
