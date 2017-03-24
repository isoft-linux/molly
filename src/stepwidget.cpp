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

#include "stepwidget.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QIcon>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QDebug>

#include "wizardwidget.h"
#include "partclonewidget.h"
#include "diskclonewidget.h"
#include "parttofilewidget.h"
#include "parttopartwidget.h"
#include "disktofilewidget.h"
#include "disktodiskwidget.h"
#include "restorewidget.h"
#include "filetopartwidget.h"
#include "partrestorewidget.h"
#include "filetodiskwidget.h"

StepWidget::StepWidget(int argc, char **argv, QWidget *parent, Qt::WindowFlags f) 
    : QWidget(parent, f) 
{
    m_UDisksClient = new UDisksClient;
    m_UDisksClient->init();

    auto *stack = new QStackedWidget;
    auto *partToFile = new PartToFileWidget(m_UDisksClient);
    connect(partToFile, &PartToFileWidget::back, [=]() { stack->setCurrentIndex(PARTCLONE); });
    auto *partToPart = new PartToPartWidget(m_UDisksClient);
    connect(partToPart, &PartToPartWidget::back, [=]() { stack->setCurrentIndex(PARTCLONE); });
    auto *partRestore = new PartRestoreWidget(m_UDisksClient);
    connect(partRestore, &PartRestoreWidget::back, [=]() { stack->setCurrentIndex(FILETOPART); });

    m_OSProber = new OSProberType("org.isoftlinux.OSProber",
                                  "/org/isoftlinux/OSProber",
                                  QDBusConnection::systemBus(),
                                  this);
    connect(m_OSProber, &OSProberType::Found, 
        [=](const QString &part, const QString &name, const QString &shortname) {
        m_OSMap[part] = name;
    });
    connect(m_OSProber, &OSProberType::Finished, [=]() {
#ifdef DEBUG
        qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << m_OSMap;
#endif
        partToFile->setOSMap(m_OSMap);
        partToPart->setOSMap(m_OSMap);
        partRestore->setOSMap(m_OSMap);
    });
    //m_OSProber->Probe();

    setWindowTitle(tr("iSOFT Partition or Disk clone and restore Assistant"));
    setWindowIcon(QIcon::fromTheme("drive-harddisk"));
    setFixedSize(625, 400);
    move((QApplication::desktop()->width() - width()) / 2, 
         (QApplication::desktop()->height() - height()) / 2);

    auto *hbox = new QHBoxLayout;
    hbox->setContentsMargins(0, 0, 0, 0);
    setLayout(hbox);
    
    auto *wizard = new WizardWidget;
    connect(wizard, &WizardWidget::next, [=](StepType type) { stack->setCurrentIndex(type); });
    auto *partClone = new PartCloneWidget;
    connect(partClone, &PartCloneWidget::back, [=]() { stack->setCurrentIndex(WIZARD); });
    connect(partClone, &PartCloneWidget::next, [=](StepType type) { stack->setCurrentIndex(type); });
    auto *diskClone = new DiskcloneWidget;
    connect(diskClone, &DiskcloneWidget::back, [=]() { stack->setCurrentIndex(WIZARD); });
    connect(diskClone, &DiskcloneWidget::next, [=](StepType type) { stack->setCurrentIndex(type); });
    auto *diskToFile = new DiskToFileWidget(m_OSMap, m_UDisksClient);
    connect(diskToFile, &DiskToFileWidget::back, [=]() { stack->setCurrentIndex(DISKCLONE); });
    auto *diskToDisk = new DiskToDiskWidget(m_OSMap, m_UDisksClient);
    connect(diskToDisk, &DiskToDiskWidget::back, [=]() { stack->setCurrentIndex(DISKCLONE); });
    auto *restore = new RestoreWidget;
    connect(restore, &RestoreWidget::back, [=]() { stack->setCurrentIndex(WIZARD); });
    connect(restore, &RestoreWidget::next, [=](StepType type) { stack->setCurrentIndex(type); });
    auto *fileToPart = new FileToPartWidget(m_UDisksClient);
    connect(fileToPart, &FileToPartWidget::back, [=]() { stack->setCurrentIndex(RESTORE); });
    connect(fileToPart, &FileToPartWidget::next, [=](QString imgPath) {
        partRestore->setImgPath(imgPath);
        stack->setCurrentIndex(PARTRESTORE);
    });
    auto *fileToDisk = new FileToDiskWidget(m_UDisksClient);
    connect(fileToDisk, &FileToDiskWidget::back, [=]() { stack->setCurrentIndex(RESTORE); });

    // TODO: add your own widget here
    stack->addWidget(wizard);           // 0
    stack->addWidget(partClone);        // 1
    stack->addWidget(diskClone);        // 2
    stack->addWidget(partToFile);       // 3
    stack->addWidget(partToPart);       // 4
    stack->addWidget(diskToFile);       // 5
    stack->addWidget(diskToDisk);       // 6
    stack->addWidget(restore);          // 7
    stack->addWidget(fileToPart);       // 8
    stack->addWidget(partRestore);      // 9
    stack->addWidget(fileToDisk);       // 10
    // for example, stack->addWidget(ditto);
    hbox->addWidget(stack);
}

StepWidget::~StepWidget()
{
    m_OSMap.clear();

    if (m_OSProber) {
        delete m_OSProber;
        m_OSProber = Q_NULLPTR;
    }

    if (m_UDisksClient) {
        delete m_UDisksClient;
        m_UDisksClient = Q_NULLPTR;
    }
}

#include "moc_stepwidget.cpp"
