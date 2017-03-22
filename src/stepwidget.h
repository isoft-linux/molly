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

#ifndef STEP_WIDGET_H
#define STEP_WIDGET_H

#include <QWidget>

#include <UDisks2Qt5/UDisksClient>

#include "osprober.h"

// TODO: add your own widget (add to stack) order here
typedef enum {
    WIZARD = 0,
    PARTCLONE,
    DISKCLONE,
    PARTTOFILE,
    PARTTOPART,
    DISKTOFILE,
    DISKTODISK,
    RESTORE,
    FILETOPART,
    PARTRESTORE,
    FILETODISK,
} StepType;

using OSProberType = org::isoftlinux::OSProber;
using OSMapType = QMap<QString, QString>;

const QString udisksDBusPathPrefix = "/org/freedesktop/UDisks2/block_devices/";
const QString partImgExt = ".part";
const QString osProberMountPoint = "/var/lib/os-prober/mount";

class StepWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StepWidget(int argc, 
                        char **argv, 
                        QWidget *parent = Q_NULLPTR, 
                        Qt::WindowFlags f = Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);
    virtual ~StepWidget();

private:
    OSProberType *m_OSProber = Q_NULLPTR;
    UDisksClient *m_UDisksClient = Q_NULLPTR;
    OSMapType m_OSMap;
};

#endif // STEP_WIDGET_H
