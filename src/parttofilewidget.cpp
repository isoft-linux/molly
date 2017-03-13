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

#include "parttofilewidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>

PartToFileWidget::PartToFileWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
    auto *vbox = new QVBoxLayout;
    auto *hbox = new QHBoxLayout;
    auto *label = new QLabel(tr("Please choose the Disk:"));
    hbox->addWidget(label);
    m_combo = new QComboBox;
    hbox->addWidget(m_combo);
    vbox->addLayout(hbox);
    m_table = new QTableWidget;
    vbox->addWidget(m_table);
    hbox = new QHBoxLayout;
    label = new QLabel(tr("Partition image save path:"));
    hbox->addWidget(label);
    auto *edit = new QLineEdit;
    hbox->addWidget(edit);
    auto *browseBtn = new QPushButton(tr("Browse"));
    hbox->addWidget(browseBtn);
    vbox->addLayout(hbox);
    hbox = new QHBoxLayout;
    auto *cloneBtn = new QPushButton(tr("Clone"));
    connect(cloneBtn, &QPushButton::clicked, [=]() {});
    hbox->addWidget(cloneBtn);
    auto *backBtn = new QPushButton(tr("Back"));
    connect(backBtn, &QPushButton::clicked, [=]() { Q_EMIT back(); });
    hbox->addWidget(backBtn);
    vbox->addLayout(hbox);
    setLayout(vbox);
}

PartToFileWidget::~PartToFileWidget()
{
    if (m_combo) {
        delete m_combo;
        m_combo = Q_NULLPTR;
    }
}

#include "moc_parttofilewidget.cpp"
