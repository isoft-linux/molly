/*
 * Copyright (C) 2017 Leslie Zhai <xiang.zhai@i-soft.com.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

#ifndef TEST_UDISKSCLIENT_GUI_H
#define TEST_UDISKSCLIENT_GUI_H

#include <QtCore/QObject>
#include <QComboBox>
#include <QTableWidget>

#include <UDisks2Qt5/UDisksClient>

#include "osprober.h"

using OSProberType = org::isoftlinux::OSProber; 

class TestUDisksClientGui : public QObject
{
    Q_OBJECT

public:
    TestUDisksClientGui();
    ~TestUDisksClientGui();

private slots:
    void testGetDriveObjects();

private:
    void getDriveObjects(QComboBox *combo, QTableWidget *table);
    void comboTextChanged(QComboBox *combo, QString text, QTableWidget *table);

private:
    UDisksClient *m_UDisksClient = nullptr;
    OSProberType *m_OSProber = nullptr;
    QMap<QString, QString> m_OSMap;
};

#endif // TEST_UDISKSCLIENT_GUI_H
