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

#include "stepwidget.h"
#include "wizardwidget.h"
#include "partclonewidget.h"
#include "diskclonewidget.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QIcon>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QDebug>

StepWidget::StepWidget(int argc, char **argv, QWidget *parent, Qt::WindowFlags f) 
    : QWidget(parent, f) 
{
    setWindowTitle(tr("iSOFT Partition or Disk clone and restore Assistant"));
    setWindowIcon(QIcon::fromTheme("drive-harddisk"));
    setFixedSize(625, 400);
    move((QApplication::desktop()->width() - width()) / 2, 
         (QApplication::desktop()->height() - height()) / 2);

    auto *hbox = new QHBoxLayout;
    setLayout(hbox);
    
    auto *stack = new QStackedWidget;
    auto *wizard = new WizardWidget;
    auto *partClone = new PartcloneWidget;
    connect(partClone, &PartcloneWidget::back, [=]() { stack->setCurrentIndex(0); });
    auto *diskClone = new DiskcloneWidget;
    connect(diskClone, &DiskcloneWidget::back, [=]() { stack->setCurrentIndex(0); });
    stack->addWidget(wizard);       // 0
    stack->addWidget(partClone);    // 1
    stack->addWidget(diskClone);    // 2
    connect(wizard, &WizardWidget::next, [=](CloneType type) { stack->setCurrentIndex(type); });
    hbox->addWidget(stack);
}

StepWidget::~StepWidget()
{
}

#include "moc_stepwidget.cpp"
