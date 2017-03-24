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

#ifndef FILETODISK_WIDGET_H
#define FILETODISK_WIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QComboBox>
#include <UDisks2Qt5/UDisksClient>
#include <UDisks2Qt5/UDisksPartition>
#include <UDisks2Qt5/UDisksBlock>
#include <UDisks2Qt5/UDisksFilesystem>
#include <UDisks2Qt5/UDisksDrive>
class FileToDiskWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FileToDiskWidget(UDisksClient *oUDisksClient,
                              QWidget *parent = Q_NULLPTR,
                              Qt::WindowFlags f = Qt::Tool);
    virtual ~FileToDiskWidget();

private:
    QTreeWidget *m_treeWidget = Q_NULLPTR;
    QPushButton *m_browseBtn = Q_NULLPTR;
    QPushButton *m_cloneBtn = Q_NULLPTR;
    UDisksClient *m_UDisksClient = Q_NULLPTR;
    QComboBox *m_combo = Q_NULLPTR;
    void getDriveObjects();
    void comboTextChanged(QString text);
Q_SIGNALS:
    void next();
    void back();

private slots:
    void setSelectedItem(QTreeWidgetItem *item,int index);
};

#endif // FILETODISK_WIDGET_H
