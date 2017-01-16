/*
 * Copyright (C) 2017 Leslie Zhai <xiang.zhai@i-soft.com.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

#ifndef TEST_PARTCLONE_H
#define TEST_PARTCLONE_H

#include <QtCore/QObject>

class TestPartclone : public QObject
{
    Q_OBJECT

public:
    TestPartclone();

private slots:
    void testParseOption1(); // -h
    void testParseOption2(); // -d -c -s /dev/sda1 -o /data/backup/sda1.img
};

#endif // TEST_PARTCLONE_H
