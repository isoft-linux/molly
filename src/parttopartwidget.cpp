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

#include "parttopartwidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>

#include <UDisks2Qt5/UDisksObject>
#include <UDisks2Qt5/UDisksDrive>

PartToPartWidget::PartToPartWidget(UDisksClient *oUDisksClient, 
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
    m_fromCombo = new QComboBox;
    vbox->addWidget(m_fromCombo);
    m_fromTable = new QTableWidget;
    QStringList headers {tr("Partition"), tr("Size"), tr("Type"), tr("OS"), tr("Status")};
    m_fromTable->setColumnCount(headers.size());
    m_fromTable->setHorizontalHeaderLabels(headers);
    m_fromTable->verticalHeader()->setVisible(false);
    m_fromTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fromTable->setSelectionMode(QAbstractItemView::SingleSelection);
    vbox->addWidget(m_fromTable);
    m_toCombo = new QComboBox;
    vbox->addWidget(m_toCombo);
    m_toTable = new QTableWidget;
    m_toTable->setColumnCount(headers.size());
    m_toTable->setHorizontalHeaderLabels(headers);
    m_toTable->verticalHeader()->setVisible(false);
    m_toTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_toTable->setSelectionMode(QAbstractItemView::SingleSelection);
    vbox->addWidget(m_toTable);
    auto *hbox = new QHBoxLayout;
    vbox->addLayout(hbox);
    auto *confirmBtn = new QPushButton(tr("Confirm"));
    confirmBtn->setEnabled(false);
    hbox->addWidget(confirmBtn);
    auto *backBtn = new QPushButton(tr("Back"));
    hbox->addWidget(backBtn);
    connect(backBtn, &QPushButton::clicked, [=]() { Q_EMIT back(); });
    setLayout(vbox);
}

PartToPartWidget::~PartToPartWidget()
{
}

bool PartToPartWidget::isPartAbleToShow(const UDisksPartition *part, 
                                        UDisksBlock *blk,
                                        UDisksFilesystem *fsys, 
                                        QTableWidgetItem *item) 
{
#ifdef DEBUG
    if (fsys)
        qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << fsys->mountPoints();
#endif
    if (!part || part->isContainer() || !blk || blk->idType() == "swap" || 
        !fsys || (!fsys->mountPoints().isEmpty() && !fsys->mountPoints().contains("/var/lib/os-prober/mount"))) {
        item->setFlags(Qt::NoItemFlags);
        return false;
    }

    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    return true;
}

void PartToPartWidget::comboTextChanged(QTableWidget *table, 
                                        QComboBox *combo, 
                                        QString text) 
{
    if (!table || !combo || text.isEmpty())
        return;

    table->setRowCount(0);
    table->clearContents();

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
#ifdef DEBUG
        qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << fsys;
#endif

        table->insertRow(row);
        QString partStr = text + QString::number(part->number());
        auto *item = new QTableWidgetItem(partStr);
        isPartAbleToShow(part, blk, fsys, item);
        table->setItem(row, 0, item);

        item = new QTableWidgetItem(QString::number(part->size() / 1073741824.0, 'f', 1) + " G");
        isPartAbleToShow(part, blk, fsys, item);
        table->setItem(row, 1, item);

        item = new QTableWidgetItem(blk->idType());
        isPartAbleToShow(part, blk, fsys, item);
        table->setItem(row, 2, item);

#ifdef DEBUG
        qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << m_OSMap << partStr;
#endif
        item = new QTableWidgetItem(m_OSMap[partStr]);
        isPartAbleToShow(part, blk, fsys, item);
        table->setItem(row, 3, item);

        item = new QTableWidgetItem("");
        isPartAbleToShow(part, blk, fsys, item);
        table->setItem(row, 4, item);
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
                    combo->setToolTip(drv->id());
            }
        }
    }
}

void PartToPartWidget::getDriveObjects() 
{
    m_fromCombo->clear();
    m_toCombo->clear();

    for (const UDisksObject::Ptr drvPtr : m_UDisksClient->getObjects(UDisksObject::Drive)) {
        UDisksDrive *drv = drvPtr->drive();
        if (!drv)
            continue;
        UDisksBlock *blk = drv->getBlock();
        if (!blk)
            continue;
        m_fromCombo->addItem(blk->preferredDevice());
        m_toCombo->addItem(blk->preferredDevice());
    }

    connect(m_fromCombo, &QComboBox::currentTextChanged, [=](const QString &text) {
        comboTextChanged(m_fromTable, m_fromCombo, text);
    });
    comboTextChanged(m_fromTable, m_fromCombo, m_fromCombo->currentText());

    connect(m_toCombo, &QComboBox::currentTextChanged, [=](const QString &text) {
        comboTextChanged(m_toTable, m_toCombo, text);
    });
    comboTextChanged(m_toTable, m_toCombo, m_toCombo->currentText());
}

void PartToPartWidget::setOSMap(OSMapType OSMap) 
{
    m_OSMap = OSMap;
    comboTextChanged(m_fromTable, m_fromCombo, m_fromCombo->currentText());
    comboTextChanged(m_toTable, m_toCombo, m_toCombo->currentText());
}

#include "moc_parttopartwidget.cpp"
