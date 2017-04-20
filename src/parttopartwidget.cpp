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
#include <QProgressBar>

#include <UDisks2Qt5/UDisksObject>
#include <UDisks2Qt5/UDisksDrive>

#include <pthread.h>
#include <libpartclone.h>

static QProgressBar *m_progress = Q_NULLPTR;
static pthread_t m_thread;
static pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *callBack(void *percent, void *remaining);

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
    m_progress = new QProgressBar;
    m_progress->setTextVisible(true);
    m_progress->setVisible(false);
    vbox->addWidget(m_progress);
    auto *hbox = new QHBoxLayout;
    vbox->addLayout(hbox);
    auto *confirmBtn = new QPushButton(tr("Confirm"));
    connect(m_fromTable, &QTableWidget::itemSelectionChanged, [=]() {
        QList<QTableWidgetItem *> items = m_fromTable->selectedItems();
        if (items.size() > 2) {
            bool ok;
            m_fromPart = items[0]->text();
            m_fromType = items[2]->text();
            m_fromSize = items[1]->text().toLongLong(&ok);
            comboTextChanged(m_toTable, m_toCombo, m_toCombo->currentText(), PARTTO);
        } else {
            m_fromSize = 0ULL;
        }
    });
    connect(m_toTable, &QTableWidget::itemSelectionChanged, [=]() {
        QList<QTableWidgetItem *> items = m_toTable->selectedItems();
        bool ok;
#ifdef DEBUG
            qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << m_fromPart << items[0]->text() << m_fromType << items[2]->text() << m_fromSize << items[1]->text().toLongLong(&ok);
#endif
        if (items.size() > 2 && m_fromPart != items[0]->text() && 
            m_fromType == items[2]->text() && 
            m_fromSize <= items[1]->text().toLongLong(&ok)) {
            confirmBtn->setEnabled(true);
        } else {
            confirmBtn->setEnabled(false);
        }
    });
    confirmBtn->setEnabled(false);
    hbox->addWidget(confirmBtn);
    auto *backBtn = new QPushButton(tr("Back"));
    connect(confirmBtn, &QPushButton::clicked, [=]() {
        m_progress->setVisible(true);
        m_progress->setValue(0);
        m_fromCombo->setEnabled(false);
        m_fromTable->setEnabled(false);
        m_toCombo->setEnabled(false);
        m_toTable->setEnabled(false);
        confirmBtn->setEnabled(false);
        backBtn->setEnabled(false);
        pthread_create(&m_thread, NULL, startRoutine, this);
    });
    hbox->addWidget(backBtn);
    connect(backBtn, &QPushButton::clicked, [=]() { Q_EMIT back(); });
    setLayout(vbox);
    
    connect(this, &PartToPartWidget::error, [=](QString message) {
        m_isError = true;
#ifdef DEBUG
        qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << m_isError << message;
#endif
        QList<QTableWidgetItem *> items = m_toTable->selectedItems();
        if (items.size() > 4) {
            items[4]->setText(tr("Error"));
        }
        m_progress->setVisible(false);
        m_fromCombo->setEnabled(true);
        m_fromTable->setEnabled(true);
        m_toCombo->setEnabled(true);
        m_toTable->setEnabled(true);
        confirmBtn->setEnabled(true);
        backBtn->setEnabled(true);
    });
    connect(this, &PartToPartWidget::finished, [=]() {
#ifdef DEBUG
        qDebug() << "DEBUG:" << __PRETTY_FUNCTION__;
#endif
        QList<QTableWidgetItem *> items = m_toTable->selectedItems();
        if (!m_isError && items.size() > 4) {
            items[4]->setText(tr("Finished"));
        }
        m_progress->setVisible(false);
        m_fromCombo->setEnabled(true);
        m_fromTable->setEnabled(true);
        m_toCombo->setEnabled(true);
        m_toTable->setEnabled(true);
        confirmBtn->setEnabled(true);
        backBtn->setEnabled(true);
    });
}

PartToPartWidget::~PartToPartWidget()
{
}

bool PartToPartWidget::isPartAbleToShow(const UDisksPartition *part, 
                                        UDisksBlock *blk,
                                        UDisksFilesystem *fsys, 
                                        QTableWidgetItem *item, 
                                        QString partStr, 
                                        PartType type) 
{
#ifdef DEBUG
    if (fsys)
        qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << fsys->mountPoints();
#endif
    if (!part || part->isContainer() || (type == PARTTO && part->size() < m_fromSize) || 
        !blk || blk->idType() == "swap" || 
        !fsys || (!fsys->mountPoints().isEmpty() && !fsys->mountPoints().contains(osProberMountPoint)) || 
        (type == PARTTO && !m_fromType.isEmpty() && m_fromType != blk->idType()) || 
        (type == PARTTO && !m_fromPart.isEmpty() && m_fromPart == partStr)) {
        item->setFlags(Qt::NoItemFlags);
        return false;
    }

    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    return true;
}

void PartToPartWidget::comboTextChanged(QTableWidget *table, 
                                        QComboBox *combo, 
                                        QString text, 
                                        PartType type) 
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
        isPartAbleToShow(part, blk, fsys, item, partStr, type);
        table->setItem(row, 0, item);

        item = new QTableWidgetItem(QString::number(part->size()));
        isPartAbleToShow(part, blk, fsys, item, partStr, type);
        table->setItem(row, 1, item);

        item = new QTableWidgetItem(blk->idType());
        isPartAbleToShow(part, blk, fsys, item, partStr, type);
        table->setItem(row, 2, item);

#ifdef DEBUG
        qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << m_OSMap << partStr;
#endif
        item = new QTableWidgetItem(m_OSMap[partStr]);
        isPartAbleToShow(part, blk, fsys, item, partStr, type);
        table->setItem(row, 3, item);

        item = new QTableWidgetItem("");
        isPartAbleToShow(part, blk, fsys, item, partStr, type);
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
        comboTextChanged(m_toTable, m_toCombo, text, PARTTO);
    });
    comboTextChanged(m_toTable, m_toCombo, m_toCombo->currentText(), PARTTO);
}

void PartToPartWidget::setOSMap(OSMapType OSMap) 
{
    m_OSMap = OSMap;
    comboTextChanged(m_fromTable, m_fromCombo, m_fromCombo->currentText());
    comboTextChanged(m_toTable, m_toCombo, m_toCombo->currentText());
}

static void *callBack(void *percent, void *remaining) 
{
    pthread_mutex_trylock(&m_mutex);
    float *value = (float *)percent;
    char *str = (char *)remaining;
    if (m_progress) {
        m_progress->setValue((int)*value);
        m_progress->setFormat(QString::number((int)*value) + "% " + QString(str));
    }
    pthread_mutex_unlock(&m_mutex);
    return Q_NULLPTR;
}

void *PartToPartWidget::errorRoutine(void *arg, void *msg) 
{
    PartToPartWidget *thisPtr = (PartToPartWidget *)arg;
    if (!thisPtr)
        return Q_NULLPTR;

    char *str = (char *)msg;
    Q_EMIT thisPtr->error(str ? QString(str) : "");

    return Q_NULLPTR;
}

void *PartToPartWidget::startRoutine(void *arg) 
{
    PartToPartWidget *thisPtr = (PartToPartWidget *)arg;
    if (!thisPtr || thisPtr->m_fromPart.isEmpty() || !thisPtr->m_toTable || 
        !thisPtr->m_UDisksClient)
        return Q_NULLPTR;
    partType type = LIBPARTCLONE_UNKNOWN;
    QList<QTableWidgetItem *> items = thisPtr->m_toTable->selectedItems();
    if (items.isEmpty())
        return Q_NULLPTR;
    QString img = items[0]->text();

    if (img.isEmpty())
        return Q_NULLPTR;
    UDisksObject::Ptr blkPtr = thisPtr->m_UDisksClient->getObject(QDBusObjectPath(udisksDBusPathPrefix + thisPtr->m_fromPart.mid(5)));
    if (!blkPtr)
        return Q_NULLPTR;
    UDisksBlock *blk = blkPtr->block();
    if (!blk)
        return Q_NULLPTR;
    QString strType = blk->idType();
    if (strType.startsWith("ext"))
        type = LIBPARTCLONE_EXTFS;
    else if (strType == "ntfs")
        type = LIBPARTCLONE_NTFS;
    else if (strType == "vfat")
        type = LIBPARTCLONE_FAT;
    // TODO: more file system test
    partClone(type, 
              thisPtr->m_fromPart.toStdString().c_str(), 
              img.toStdString().c_str(),
              1,
              callBack,
              errorRoutine,
              thisPtr);
    Q_EMIT thisPtr->finished();
    return Q_NULLPTR;
}

#include "moc_parttopartwidget.cpp"
