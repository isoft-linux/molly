/*
 * Copyright (C) 2017 Leslie Zhai <xiang.zhai@i-soft.com.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

#include <QtTest/QTest>

#include "test-partclone.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "partclone.h"
#ifdef __cplusplus
}
#endif

QTEST_GUILESS_MAIN(TestPartclone)

TestPartclone::TestPartclone()
{
}

void TestPartclone::testParseOption1() 
{
#if 0
    const char* argv[] = { "-h" };
    int argc = sizeof(argv) / sizeof(char*);
    cmd_opt opt;
    parse_options(argc, (char**)argv, &opt);
#endif
}

void TestPartclone::testParseOption2()
{
    const char* argv[] = { "-d", "-c", "-s", "/dev/sda1", "-o", "/data/backup/sda1.img" };
    int argc = sizeof(argv) / sizeof(char*);
    cmd_opt opt;
    parse_options(argc, (char**)argv, &opt);
}
