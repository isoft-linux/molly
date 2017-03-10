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

#include "wizardwidget.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QPainter>
#include <QDebug>

WizardWidget::WizardWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
    auto *vbox = new QVBoxLayout;
    auto *label = new QLabel(tr("Welcome to use iSOFT Partition or Disk clone "
                                "and restore Assistant"));
    label->setAlignment(Qt::AlignHCenter);
    vbox->addWidget(label);
    auto *hbox = new QHBoxLayout;
    vbox->addLayout(hbox);
	auto *cloneMenu = new QMenu;
    cloneMenu->addAction(tr("Partition Clone"), [=]() {
        Q_EMIT next(PARTITION_CLONE);
    });
    cloneMenu->addAction(tr("Disk Clone"), [=]() {
        Q_EMIT next(DISK_CLONE);
    });
    auto *cloneBtn = new QPushButton(tr("Clone"));
    cloneBtn->setMenu(cloneMenu);
    hbox->addWidget(cloneBtn);
    auto *restoreMenu = new QMenu;
    restoreMenu->addAction(tr("Partition Image Restore"));
    restoreMenu->addAction(tr("Disk Image Restore"));
    auto *restoreBtn = new QPushButton(tr("Restore"));
    restoreBtn->setMenu(restoreMenu);
    hbox->addWidget(restoreBtn);
    setLayout(vbox);
}

WizardWidget::~WizardWidget()
{
}

void WizardWidget::paintEvent(QPaintEvent *event) 
{
    QPainter painter(this);
    QImage image(":/data/background.png");
    painter.drawImage(0, 0, image.scaled(width(), height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

#include "moc_wizardwidget.cpp"
