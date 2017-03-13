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

#include "parttofilewidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QHeaderView>

#include <UDisks2Qt5/UDisksObject>
#include <UDisks2Qt5/UDisksDrive>

PartToFileWidget::PartToFileWidget(OSProberType *OSProber, 
                                   QWidget *parent, 
                                   Qt::WindowFlags f)
    : QWidget(parent, f), 
      m_OSProber(OSProber)
{
    m_UDisksClient = new UDisksClient;
    m_UDisksClient->init();
    connect(m_UDisksClient, &UDisksClient::objectAdded, [=](const UDisksObject::Ptr &object) {
        getDriveObjects();
        m_OSMap.clear();
        m_OSProber->Probe();
    });

    connect(m_UDisksClient, &UDisksClient::objectRemoved, [=](const UDisksObject::Ptr &object) {
        getDriveObjects();
        m_OSMap.clear();
        m_OSProber->Probe();
    });

    connect(m_UDisksClient, &UDisksClient::objectsAvailable, [=]() {
        getDriveObjects();
    });

    auto *vbox = new QVBoxLayout;
    auto *hbox = new QHBoxLayout;
    auto *label = new QLabel(tr("Please choose the Disk:"));
    hbox->addWidget(label);
    m_combo = new QComboBox;
    hbox->addWidget(m_combo);
    vbox->addLayout(hbox);
    m_table = new QTableWidget;
    QStringList headers {tr("Partition"), tr("Size"), tr("Type"), tr("OS")};
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    vbox->addWidget(m_table);
    hbox = new QHBoxLayout;
    label = new QLabel(tr("Partition image save path:"));
    hbox->addWidget(label);
    auto *edit = new QLineEdit;
    hbox->addWidget(edit);
    auto *browseBtn = new QPushButton(tr("Browse"));
    hbox->addWidget(browseBtn);
    vbox->addLayout(hbox);
    hbox = new QHBoxLayout;
    auto *cloneBtn = new QPushButton(tr("Clone"));
    connect(cloneBtn, &QPushButton::clicked, [=]() {});
    hbox->addWidget(cloneBtn);
    auto *backBtn = new QPushButton(tr("Back"));
    connect(backBtn, &QPushButton::clicked, [=]() { Q_EMIT back(); });
    hbox->addWidget(backBtn);
    vbox->addLayout(hbox);
    setLayout(vbox);
}

PartToFileWidget::~PartToFileWidget()
{
    if (m_UDisksClient) {
        delete m_UDisksClient;
        m_UDisksClient = Q_NULLPTR;
    }

    if (m_combo) {
        delete m_combo;
        m_combo = Q_NULLPTR;
    }
}

bool PartToFileWidget::isPartAbleToShow(const UDisksPartition *part, 
                                        UDisksBlock *blk,
                                        UDisksFilesystem *fsys, 
                                        QTableWidgetItem *item) 
{
    if (part->isContainer() || !blk || blk->idType() == "swap" || !fsys || 
        !fsys->mountPoints().isEmpty()) {
        item->setFlags(Qt::NoItemFlags);
        return false;
    }

    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    return true;
}

void PartToFileWidget::comboTextChanged(QString text) 
{
    if (text.isEmpty())
        return;

    m_table->setRowCount(0);
    m_table->clearContents();

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

        item = new QTableWidgetItem(m_OSMap[partStr]);
        isPartAbleToShow(part, blk, fsys, item);
        m_table->setItem(row, 3, item);
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

void PartToFileWidget::getDriveObjects() 
{
    m_combo->clear();

    for (const UDisksObject::Ptr drvPtr : m_UDisksClient->getObjects(UDisksObject::Drive)) {
        UDisksDrive *drv = drvPtr->drive();
        if (drv == nullptr)
            continue;
        UDisksBlock *blk = drv->getBlock();
        if (blk == nullptr)
            continue;
        m_combo->addItem(blk->preferredDevice());
    }

    connect(m_combo, &QComboBox::currentTextChanged, [=](const QString &text) {
        comboTextChanged(text);
    });
    comboTextChanged(m_combo->currentText());
}

#include "moc_parttofilewidget.cpp"
