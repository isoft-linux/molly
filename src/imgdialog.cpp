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

#include "imgdialog.h"
#include "parttofilewidget.h"

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QFileDialog>
#include <QDebug>

#include <UDisks2Qt5/UDisksDrive>

PrevWidget::PrevWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
    auto *vbox = new QVBoxLayout;
    list = new QListWidget;
    vbox->addWidget(list);
    list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setWrapping(false);
    list->setViewMode(QListView::IconMode);
    list->setIconSize(QSize(96, 84));
    list->setSpacing(12);
    connect(list, &QListWidget::itemClicked, [=](QListWidgetItem *item) {
        if (item)
            Q_EMIT next(item->data(Qt::WhatsThisRole).toString());
    });
    auto *label = new QLabel(tr("<a href=\"#\">Other</a>"));
    connect(label, &QLabel::linkActivated, [=]() { Q_EMIT other(); });
    vbox->addWidget(label);
    setLayout(vbox);
}

PrevWidget::~PrevWidget()
{
    if (list) {
        delete list;
        list = Q_NULLPTR;
    }
}

NextWidget::NextWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
    auto *vbox = new QVBoxLayout;
    table = new QTableWidget;
    QStringList headers {tr("Partition"), tr("Size"), tr("Type"), tr("OS")};
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    vbox->addWidget(table);
    auto *hbox = new QHBoxLayout;
    vbox->addLayout(hbox);
    auto *prevBtn = new QPushButton(tr("Previous"));
    connect(prevBtn, &QPushButton::clicked, [=]() { Q_EMIT prev(); });
    hbox->addWidget(prevBtn);
    setLayout(vbox);
}

NextWidget::~NextWidget()
{
    if (table) {
        delete table;
        table = Q_NULLPTR;
    }
}

ImgDialog::ImgDialog(QString selected, 
                     QMap<QString, QString> OSMap, 
                     QString title, 
                     QWidget *parent, 
                     Qt::WindowFlags f)
    : QDialog(parent, f), 
      m_OSMap(OSMap) 
{
    m_UDisksClient = new UDisksClient;
    m_UDisksClient->init();
    connect(m_UDisksClient, &UDisksClient::objectAdded, [=](const UDisksObject::Ptr &object) {
        getDriveObjects();
    });
    connect(m_UDisksClient, &UDisksClient::objectRemoved, [=](const UDisksObject::Ptr &object) {
        getDriveObjects();
    });
    connect(m_UDisksClient, &UDisksClient::objectsAvailable, [=]() {
        UDisksObject::Ptr selectedObj = m_UDisksClient->getObject(QDBusObjectPath(udisksDBusPathPrefix + selected.mid(5)));
        if (selectedObj) {
            UDisksPartition *selectedPart = selectedObj->partition();
            if (selectedPart) {
                m_selectedSize = selectedPart->size();
            }
        }
#ifdef DEBUG
        qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << selected << m_selectedSize;
#endif
        getDriveObjects();
    });

    setWindowTitle(title);
    setModal(true);
    setFixedSize(447, 230);
    auto *vbox = new QVBoxLayout;
    auto *stack = new QStackedWidget;
    vbox->addWidget(stack);
    m_prev = new PrevWidget;
    m_next = new NextWidget;
    connect(m_prev, &PrevWidget::other, [=]() {
        hide();
        auto *dlg = new QFileDialog;
        dlg->show();
    });
    connect(m_prev, &PrevWidget::next, [=](QString text) { 
        stack->setCurrentIndex(1);
        if (!m_next || !m_next->table)
            return;
        m_next->table->setRowCount(0);
        m_next->table->clearContents();
#ifdef DEBUG
        qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << text;
#endif
        QDBusObjectPath tblPath = QDBusObjectPath(udisksDBusPathPrefix + text.mid(5));
        QList<UDisksPartition *> parts;
        for (const UDisksObject::Ptr partPtr : m_UDisksClient->getPartitions(tblPath)) {
            UDisksPartition *part = partPtr->partition();
            if (selected == text + QString::number(part->number()) || 
                part->size() < m_selectedSize) {
                continue;
            }
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

            m_next->table->insertRow(row);
            QString partStr = text + QString::number(part->number());
            auto *item = new QTableWidgetItem(partStr);
            PartToFileWidget::isPartAbleToShow(part, blk, fsys, item);
            m_next->table->setItem(row, 0, item);

            item = new QTableWidgetItem(QString::number(part->size() / 1073741824.0, 'f', 1) + " G");
            PartToFileWidget::isPartAbleToShow(part, blk, fsys, item);
            m_next->table->setItem(row, 1, item);

            item = new QTableWidgetItem(blk->idType());
            PartToFileWidget::isPartAbleToShow(part, blk, fsys, item);
            m_next->table->setItem(row, 2, item);

            item = new QTableWidgetItem(m_OSMap[partStr]);
            PartToFileWidget::isPartAbleToShow(part, blk, fsys, item);
            m_next->table->setItem(row, 3, item);
            row++;
        }
    });
    connect(m_next, &NextWidget::prev, [=]() { stack->setCurrentIndex(0); });
    stack->addWidget(m_prev);
    stack->addWidget(m_next);
    setLayout(vbox);
}

ImgDialog::~ImgDialog()
{
    if (m_UDisksClient) {
        delete m_UDisksClient;
        m_UDisksClient = Q_NULLPTR;
    }

    if (m_prev) {
        delete m_prev;
        m_prev = Q_NULLPTR;
    }

    if (m_next) {
        delete m_next;
        m_next = Q_NULLPTR;
    }
}

void ImgDialog::getDriveObjects() 
{
    if (!m_prev || !m_prev->list)
        return;

    m_prev->list->clear();
    for (const UDisksObject::Ptr drvPtr : m_UDisksClient->getObjects(UDisksObject::Drive)) {
        UDisksDrive *drv = drvPtr->drive();
        if (!drv)
            continue;
        UDisksBlock *blk = drv->getBlock();
        if (!blk)
            continue;
        if (blk->size() < m_selectedSize)
            continue;
        auto *item = new QListWidgetItem(QIcon::fromTheme("drive-harddisk"), 
            blk->preferredDevice() + "\n" + drv->id() + "\n" + QString::number(blk->size() / 1073741824.0, 'f', 1) + " G", 
            m_prev->list);
        item->setData(Qt::WhatsThisRole, blk->preferredDevice());
    }
}

#include "moc_imgdialog.cpp"
