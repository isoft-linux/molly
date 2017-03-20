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

#include "filetopartwidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QDir>
#include <QLineEdit>
#include <QFileDialog>

#include <UDisks2Qt5/UDisksDrive>
#include <UDisks2Qt5/UDisksPartition>
#include <UDisks2Qt5/UDisksBlock>
#include <UDisks2Qt5/UDisksFilesystem>

#include <libpartclone.h>

FileToPartWidget::FileToPartWidget(UDisksClient *oUDisksClient, 
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

    auto *vbox = new QVBoxLayout;
    auto *hbox = new QHBoxLayout;
    vbox->addLayout(hbox);
    auto *label = new QLabel(tr("Partition Image file save path:"));
    hbox->addWidget(label);
    m_combo = new QComboBox;
    hbox->addWidget(m_combo);
    vbox->addLayout(hbox);
    m_table = new QTableWidget;
    QStringList headers {tr("Image"), tr("Type"), tr("Size")};
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    auto *edit = new QLineEdit;
    auto *nextBtn = new QPushButton(tr("Next"));
    nextBtn->setEnabled(false);
    connect(nextBtn, &QPushButton::clicked, [=]() {
        QString imgPath = edit->text();
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        if (imgPath.isEmpty()) {
            if (items.size())
                imgPath = items[0]->text();
        }
        QFileInfo info(imgPath);
        if (info.isReadable()) {
#ifdef DEBUG
            qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << imgPath;
#endif
            Q_EMIT next(imgPath);
        }
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, [=]() {
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        if (items.size()) {
            QFileInfo info(items[0]->text());
            if (info.isReadable())
                nextBtn->setEnabled(true);
        }
    });
    vbox->addWidget(m_table);
    hbox = new QHBoxLayout;
    vbox->addLayout(hbox);
    connect(edit, &QLineEdit::textChanged, [=](const QString &text) {
        QFileInfo info(text);
        if (info.isReadable())
            nextBtn->setEnabled(true);
    });
    hbox->addWidget(edit);
    auto *browseBtn = new QPushButton(tr("Browse"));
    hbox->addWidget(browseBtn);
    connect(browseBtn, &QPushButton::clicked, [=]() {
        QString fileName = QFileDialog::getOpenFileName(this, tr("Open Image File"), 
                QStandardPaths::writableLocation(QStandardPaths::HomeLocation), 
                tr("Image Files (*.part)"));
        edit->setText(fileName);
	});
    hbox = new QHBoxLayout;
    vbox->addLayout(hbox);
    hbox->addWidget(nextBtn);
    auto *backBtn = new QPushButton(tr("Back"));
    connect(backBtn, &QPushButton::clicked, [=]() { Q_EMIT back(); });
    hbox->addWidget(backBtn);
    setLayout(vbox);
}

FileToPartWidget::~FileToPartWidget()
{
    if (m_combo) {
        delete m_combo;
        m_combo = Q_NULLPTR;
    }

    if (m_table) {
        delete m_table;
        m_table = Q_NULLPTR;
    }
}

void FileToPartWidget::comboTextChanged(QString text) 
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
        if (!fsys)
            continue;

        for (QString mount : fsys->mountPoints()) {
            if (mount == "/var/lib/os-prober/mount")
                continue;
            QDir imgDir = QDir(mount, "*" + partImgExt, QDir::Name, QDir::Files | QDir::NoSymLinks | QDir::Readable);
            if (mount == "/") {
                QStringList locations = QStandardPaths::standardLocations(QStandardPaths::HomeLocation);
                if (locations.size())
                    imgDir.setPath(locations[0]);
            }
#ifdef DEBUG
            qDebug() << "DEBUG:" << imgDir;
#endif
            for (QString name : imgDir.entryList()) {
                QString imgPath = imgDir.path() + "/" + name;
                partInfo_t info;
                partInfo(imgPath.toStdString().c_str(), &info);
                m_table->insertRow(row);
                auto *item = new QTableWidgetItem(imgPath);
                m_table->setItem(row, 0, item);

                item = new QTableWidgetItem(QString(info.type));
                m_table->setItem(row, 1, item);

                item = new QTableWidgetItem(QString(info.devSize));
                m_table->setItem(row, 2, item);
                row++;
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

void FileToPartWidget::getDriveObjects() 
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

#include "moc_filetopartwidget.cpp"
