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

#ifndef PARTTOPART_WIDGET_H
#define PARTTOPART_WIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QComboBox>

#include <UDisks2Qt5/UDisksClient>
#include <UDisks2Qt5/UDisksPartition>
#include <UDisks2Qt5/UDisksBlock>
#include <UDisks2Qt5/UDisksFilesystem>

#include "stepwidget.h"

class PartToPartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PartToPartWidget(UDisksClient *oUDisksClient, 
                              QWidget *parent = Q_NULLPTR, 
                              Qt::WindowFlags f = Qt::Tool);
    virtual ~PartToPartWidget();
    void setOSMap(OSMapType OSMap);

Q_SIGNALS:
    void next();
    void back();

private:
    void getDriveObjects();
    void comboTextChanged(QTableWidget *table, 
                          QComboBox *combo, 
                          QString text);
    bool isPartAbleToShow(const UDisksPartition *part, 
                          UDisksBlock *blk,
                          UDisksFilesystem *fsys, 
                          QTableWidgetItem *item);

    UDisksClient *m_UDisksClient = Q_NULLPTR;
    OSMapType m_OSMap;
    QComboBox *m_fromCombo = Q_NULLPTR;
    QComboBox *m_toCombo = Q_NULLPTR;
    QTableWidget *m_fromTable = Q_NULLPTR;
    QTableWidget *m_toTable = Q_NULLPTR;
};

#endif // PARTTOPART_WIDGET_H
