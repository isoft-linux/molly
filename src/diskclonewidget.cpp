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

#include "diskclonewidget.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

DiskcloneWidget::DiskcloneWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->setAlignment(Qt::AlignHCenter);
    auto *toFileBtn = new QPushButton(QIcon(":/data/disk-to-file.png"), tr("Disk to Image File"));
    toFileBtn->setFixedSize(254, 49);
    connect(toFileBtn, &QPushButton::clicked, [=]() { Q_EMIT next(DISKTOFILE); });
    vbox->addWidget(toFileBtn);
    auto *toDiskBtn = new QPushButton(QIcon(":/data/disk-to-disk.png"), tr("Disk to Disk"));
    toDiskBtn->setFixedSize(254, 49);
    connect(toDiskBtn, &QPushButton::clicked, [=]() { Q_EMIT next(DISKTODISK); });
    vbox->addWidget(toDiskBtn);
    auto *backBtn = new QPushButton(tr("Back"));
    backBtn->setFixedSize(254, 49);

    connect(backBtn, &QPushButton::clicked, [=]() { Q_EMIT back(); });
    vbox->addWidget(backBtn);
    setLayout(vbox);
}

DiskcloneWidget::~DiskcloneWidget()
{
}

#include "moc_diskclonewidget.cpp"
