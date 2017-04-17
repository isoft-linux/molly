/*
 * Copyright (C) 2017 fj <fujiang.zhu@i-soft.com.cn>
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

#ifndef UTILS_H_FOR_MOLLY
#define UTILS_H_FOR_MOLLY
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/time.h>
#include <dirent.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/ioctl.h>
# include <linux/fs.h>
#include <libgen.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>

/*
* DISKCFGFILE:[disk to disk] create this file;[file to disk] uses it;
* DISKSTARTBIN:[disk to disk][file to disk] use it;
* CANCEL_DD_STR:[disk to file] to cancel dd.
*/
#define DISKCFGFILE ".disk.cfg"
#define DISKSTARTBIN "sdx.start.bin"
#define DISK_CLONE_EXT_NAME ".part"
#define CANCEL_DD_STR "cancel"

typedef struct {
    unsigned long long diskSize;
    char startBinMd5[128];
    int  partNumber;
} diskcfginfo_t;

extern void format_size(uint64_t size, char *result);
extern int monitor_processes(const char *cmd,char *pos,char *tsize);
extern void get_size(char src[32],uint64_t *result);
extern int setDiskCfgInfo(const char *file,diskcfginfo_t *cfgInfo);
extern int getDiskCfgInfo(const char *file,diskcfginfo_t *cfgInfo);
#endif

