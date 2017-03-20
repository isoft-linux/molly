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

#ifndef PARTTOFILE_WIDGET_H
#define PARTTOFILE_WIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QLineEdit>

#include <UDisks2Qt5/UDisksClient>
#include <UDisks2Qt5/UDisksPartition>
#include <UDisks2Qt5/UDisksBlock>
#include <UDisks2Qt5/UDisksFilesystem>

#include "stepwidget.h"

class PartToFileWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PartToFileWidget(UDisksClient *oUDisksClient, 
                              QWidget *parent = Q_NULLPTR, 
                              Qt::WindowFlags f = Qt::Tool);
    virtual ~PartToFileWidget();
    void setOSMap(OSMapType OSMap);

Q_SIGNALS:
    void back();
    void next();
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

    UDisksClient *m_UDisksClient = Q_NULLPTR;
    QComboBox *m_combo = Q_NULLPTR;
    QTableWidget *m_table = Q_NULLPTR;
    QLineEdit *m_edit = Q_NULLPTR;
    QPushButton *m_browseBtn = Q_NULLPTR;
    QPushButton *m_cloneBtn = Q_NULLPTR;
    OSMapType m_OSMap;
    bool m_isClone = true;
    bool m_isError = false;
};

#endif // PARTTOFILE_WIDGET_H
