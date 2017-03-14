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

#include "disktofilewidget.h"
#include "imgdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QHeaderView>

#include <UDisks2Qt5/UDisksObject>
#include <UDisks2Qt5/UDisksDrive>

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
    m_OSProber->Probe();

    m_UDisksClient = new UDisksClient;
    m_UDisksClient->init(); // Don't forget this!
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
    vbox->addLayout(hbox);
    m_table = new QTableWidget;
    QStringList headers {tr("Name"), tr("Disk"), tr("Serial"), tr("Size"), tr("UsedSize"), tr("State")};
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_browseBtn = new QPushButton(tr("Browse"));
    connect(m_browseBtn, &QPushButton::clicked, [=]() {
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        if (items.size()) {
            auto *imgDlg = new ImgDialog(items[0]->text(),
                                         m_OSMap,
                                         tr("Choose a Partition to save the image"));
            imgDlg->show();
        }
    });
    m_browseBtn->setEnabled(false);
    connect(m_table, &QTableWidget::itemSelectionChanged, [=]() {
        QList<QTableWidgetItem *> items = m_table->selectedItems();
        if (items.size()) {
            m_browseBtn->setEnabled(true);
        }
    });
    vbox->addWidget(m_table);
    hbox = new QHBoxLayout;
    label = new QLabel(tr("Partition image save path:"));
    hbox->addWidget(label);
    auto *edit = new QLineEdit;
    hbox->addWidget(edit);
    hbox->addWidget(m_browseBtn);
    vbox->addLayout(hbox);
    hbox = new QHBoxLayout;
    m_cloneBtn = new QPushButton(tr("Clone"));
    m_cloneBtn->setEnabled(false);
    connect(m_cloneBtn, &QPushButton::clicked, [=]() {}); // todo...
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

        int row = 0;
        m_table->insertRow(row);
        auto *item = new QTableWidgetItem(drv->id());
        m_table->setItem(row, 0, item);

        QString sdx = blk->preferredDevice();
        item = new QTableWidgetItem(sdx.mid(5));
        m_table->setItem(row, 1, item);

        item = new QTableWidgetItem(drv->serial());
        m_table->setItem(row, 2, item);

        item = new QTableWidgetItem(QString::number(blk->size() / 1073741824.0, 'f', 1) + " G");
        m_table->setItem(row, 3, item);



        item = new QTableWidgetItem(QString::number(123));
        m_table->setItem(row, 4, item);

        item = new QTableWidgetItem(QString("not backup"));
        m_table->setItem(row, 5, item);
        row++;

        printf("%d,blk size[%ull]vs drive size[%ull]||blk hintname[%s] blk id[%s]\n",
               __LINE__,
               blk->size(),drv->size(),
               qPrintable(blk->hintName()),qPrintable(blk->id()));
    }
}

#include "moc_disktofilewidget.cpp"
