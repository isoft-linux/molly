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

#ifndef PARTRESTORE_WIDGET_H
#define PARTRESTORE_WIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QTableWidget>

#include <UDisks2Qt5/UDisksClient>

#include "stepwidget.h"

class PartRestoreWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PartRestoreWidget(UDisksClient *oUDisksClient, 
                               QWidget *parent = Q_NULLPTR, 
                               Qt::WindowFlags f = Qt::Tool);
    virtual ~PartRestoreWidget();
    void setImgPath(const QString imgPath);
    void setOSMap(OSMapType OSMap);

Q_SIGNALS:
    void back();
    void error(QString message);
    void finished();

private:
    void getDriveObjects();
    void comboTextChanged(QString text);
    bool isPartAbleToShow(const UDisksPartition *part, 
                          UDisksBlock *blk,
                          UDisksFilesystem *fsys, 
                          QTableWidgetItem *item);
    static void *startRoutine(void *arg);
    static void *errorRoutine(void *arg, void *msg);

    QString m_imgPath = "";
    QString m_imgType = "";
    unsigned long long m_imgUsed = 0;
    OSMapType m_OSMap;
    UDisksClient *m_UDisksClient = Q_NULLPTR;
    QComboBox *m_combo = Q_NULLPTR;
    QTableWidget *m_table = Q_NULLPTR;
};

#endif // PARTRESTORE_WIDGET_H
