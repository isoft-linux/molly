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
#include <QHeaderView>
#include <QFileDialog>
#include <QFileInfo>
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
        if (item && !item->data(Qt::WhatsThisRole).isNull())
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

NextWidget::NextWidget(QString selected, 
                       UDisksClient *oUDisksClient, 
                       QWidget *parent, 
                       Qt::WindowFlags f)
    : QWidget(parent, f)
{
    auto *vbox = new QVBoxLayout;
    confirmBtn = new QPushButton(tr("Confirm"));
    confirmBtn->setEnabled(false);
    table = new QTableWidget;
    connect(table, &QTableWidget::itemSelectionChanged, [=]() {
        QList<QTableWidgetItem *> items = table->selectedItems();
        if (items.size()) {
            confirmBtn->setEnabled(true);
        }
    });
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
    connect(confirmBtn, &QPushButton::clicked, [=]() {
        QList<QTableWidgetItem *> items = table->selectedItems();
        if (items.size()) {
            QString selectedPart = items[0]->text();
#ifdef DEBUG
            qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << selectedPart;
#endif
            if (oUDisksClient && !selectedPart.isEmpty() && !selected.isEmpty()) {
                UDisksObject::Ptr fsysPtr = oUDisksClient->getObject(QDBusObjectPath(udisksDBusPathPrefix + selectedPart.mid(5)));
                if (fsysPtr) {
                    UDisksFilesystem *fsys = fsysPtr->filesystem();
                    if (fsys) {
                        QStringList mountPoints = fsys->mountPoints();
#ifdef DEBUG
                        qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << mountPoints;
#endif
                        if (mountPoints.size()) {
                            QString path;
                            if (mountPoints.contains("/")) {
                                path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/";
                            } else {
                                path = mountPoints[0] + "/";
                            }
                            path += selected.mid(5) + "-" + QString::number(time(Q_NULLPTR)) + partImgExt;
#ifdef DEBUG
                            qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << path;
#endif
                            if (ImgDialog::isPathWritable(path))
                                Q_EMIT savePathSelected(path);
                        }
                    }
                }
            }
        }
    });
    hbox->addWidget(prevBtn);
    hbox->addWidget(confirmBtn);
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
    m_next = new NextWidget(selected, m_UDisksClient);
    connect(m_prev, &PrevWidget::other, [=]() {
        close();
        QString path = QFileDialog::getExistingDirectory(parent, 
                                                         tr("Open Directory"), 
                                                         QStandardPaths::writableLocation(QStandardPaths::HomeLocation), 
                                                         QFileDialog::ShowDirsOnly | 
                                                         QFileDialog::DontResolveSymlinks);
        path += "/" + selected.mid(5) + "-" + QString::number(time(Q_NULLPTR)) + partImgExt;
#ifdef DEBUG
        qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << path;
#endif
        if (ImgDialog::isPathWritable(path))
            Q_EMIT savePathSelected(path);
    });
    connect(m_prev, &PrevWidget::next, [=](QString text) { 
        stack->setCurrentIndex(1);
        if (!m_next || !m_next->table || !m_next->confirmBtn)
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
            
            bool isSelected = false;
            if (selected == text + QString::number(part->number()) || 
                part->size() < m_selectedSize) {
                isSelected = true;
            }

            m_next->table->insertRow(row);
            QString partStr = text + QString::number(part->number());
            auto *item = new QTableWidgetItem(partStr);
            isPartAbleToShow(part, blk, fsys, isSelected, item);
            m_next->table->setItem(row, 0, item);

            item = new QTableWidgetItem(QString::number(part->size() / 1073741824.0, 'f', 1) + " G");
            isPartAbleToShow(part, blk, fsys, isSelected, item);
            m_next->table->setItem(row, 1, item);

            item = new QTableWidgetItem(blk->idType());
            isPartAbleToShow(part, blk, fsys, isSelected, item);
            m_next->table->setItem(row, 2, item);

            item = new QTableWidgetItem(m_OSMap[partStr]);
            isPartAbleToShow(part, blk, fsys, isSelected, item);
            m_next->table->setItem(row, 3, item);
            row++;
        }

        m_next->confirmBtn->setEnabled(false);
    });
    connect(m_next, &NextWidget::savePathSelected, [=](QString path) { Q_EMIT savePathSelected(path); });
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
        auto *item = new QListWidgetItem(QIcon(":/data/harddisk.png"), 
            blk->preferredDevice() + "\n" + drv->id() + "\n" + QString::number(blk->size() / 1073741824.0, 'f', 1) + " G", 
            m_prev->list);
        item->setData(Qt::WhatsThisRole, blk->preferredDevice());
        if (blk->size() < m_selectedSize) {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            item->setData(Qt::WhatsThisRole, QVariant());
        }
    }
}

bool ImgDialog::isPartAbleToShow(const UDisksPartition *part, 
                                 UDisksBlock *blk,
                                 UDisksFilesystem *fsys,
                                 bool isSelected, 
                                 QTableWidgetItem *item) 
{
    if (!part || part->isContainer() || !blk || blk->idType() == "swap" || 
        !fsys || fsys->mountPoints().isEmpty() || isSelected) {
        item->setFlags(Qt::NoItemFlags);
        return false;
    }

    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    return true;
}

bool ImgDialog::isPathWritable(QString path) 
{
    QFileInfo fileInfo(path);
    QFileInfo dirInfo(fileInfo.absoluteDir().path());
#ifdef DEBUG
    qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << dirInfo.path();
#endif
    return dirInfo.isWritable();
}

#include "moc_imgdialog.cpp"
