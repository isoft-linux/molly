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

#include "partclonewidget.h"

#include <QVBoxLayout>
#include <QPushButton>

PartcloneWidget::PartcloneWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->setAlignment(Qt::AlignHCenter);
    auto *toFileBtn = new QPushButton(QIcon::fromTheme("media-optical"), tr("Partition to Image File"));
    toFileBtn->setFixedSize(254, 49);
    vbox->addWidget(toFileBtn);
    auto *toPartBtn = new QPushButton(QIcon::fromTheme("drive-harddisk"), tr("Partition to Partition"));
    toPartBtn->setFixedSize(254, 49);
    vbox->addWidget(toPartBtn);
    auto *backBtn = new QPushButton(tr("Back"));
    backBtn->setFixedSize(254, 49);
    connect(backBtn, &QPushButton::clicked, [=]() { Q_EMIT back(); });
    vbox->addWidget(backBtn);
    setLayout(vbox);
}

PartcloneWidget::~PartcloneWidget()
{
}

#include "moc_partclonewidget.cpp"
