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

#include <QtGlobal>
#include <QVBoxLayout>
#include <QLabel>
#include <QSpacerItem>
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QPainter>
#include <QDebug>

WizardWidget::WizardWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
    auto *vbox = new QVBoxLayout;
    auto *spacer = new QSpacerItem(width(), 120);
    vbox->addSpacerItem(spacer);
    auto *label = new QLabel(tr("iSOFT Partition or Disk \nclone and restore Assistant"));
    label->setAlignment(Qt::AlignHCenter);
    QFont font = label->font();
    font.setPixelSize(39);
    label->setFont(font);
    QPalette palette = label->palette();
    palette.setColor(QPalette::Foreground, Qt::black);
    label->setPalette(palette);
    vbox->addWidget(label);
    spacer = new QSpacerItem(width(), 60);
    vbox->addSpacerItem(spacer);
    auto *hbox = new QHBoxLayout;
    vbox->addLayout(hbox);
	auto *cloneMenu = new QMenu;
#if QT_VERSION >= 0x050600
    cloneMenu->addAction(tr("Partition Clone"), [=]() { Q_EMIT next(PARTCLONE); });
    cloneMenu->addAction(tr("Disk Clone"), [=]() { Q_EMIT next(DISKCLONE); });
#else
    cloneMenu->addAction(tr("Partition Clone"), this, SLOT(slotNextPartClone()));
    cloneMenu->addAction(tr("Disk Clone"), this, SLOT(slotNextDiskClone()));
#endif
    spacer = new QSpacerItem(130, -1);
    hbox->addSpacerItem(spacer);
    auto *cloneBtn = new QPushButton(tr("Clone"));
    cloneBtn->setFixedSize(115, 40);
    cloneBtn->setMenu(cloneMenu);
    hbox->addWidget(cloneBtn);
    auto *restoreBtn = new QPushButton(tr("Restore"));
    connect(restoreBtn, &QPushButton::clicked, [=]() { Q_EMIT next(RESTORE); });
    restoreBtn->setFixedSize(115, 40);
    hbox->addWidget(restoreBtn);
    spacer = new QSpacerItem(130, -1);
    hbox->addSpacerItem(spacer);
    spacer = new QSpacerItem(width(), 143);
    vbox->addSpacerItem(spacer);
    setLayout(vbox);
}

WizardWidget::~WizardWidget()
{
}

#if QT_VERSION < 0x050600
void WizardWidget::slotNextPartClone() 
{
    Q_EMIT next(PARTCLONE);
}

void WizardWidget::slotNextDiskClone() 
{
    Q_EMIT next(DISKCLONE);
}
#endif

void WizardWidget::paintEvent(QPaintEvent *event) 
{
    QPainter painter(this);
    QImage image(":/data/background.png");
    painter.drawImage(0, 0, image.scaled(width(), height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

#include "moc_wizardwidget.cpp"
