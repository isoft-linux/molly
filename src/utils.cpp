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

#include "utils.h"

#define PROC_PATH       "/proc"
#define MAX_PIDS        32
#define MAX_RESULTS     32
#define MAX_FD_PER_PID  512
#define LINE_LEN        256
#define PM_NONE         0
#define PM_READ         1   // read only
#define PM_WRITE        2   // write only
#define PM_READWRITE    4

#define DIM(x) (sizeof(x)/sizeof(*(x)))

static const char     *sizes[]   = { "EB", "PB", "TB", "GB", "MB", "KB", "B" };
static const uint64_t  exbibytes = 1024ULL * 1024ULL * 1024ULL *
        1024ULL * 1024ULL * 1024ULL;

typedef struct fdinfo_t {
    int num;
    off_t size;
    off_t pos;
    char mode;
    char name[MAXPATHLEN + 1];
    struct timeval tv;
} fdinfo_t;
typedef struct pidinfo_t {
    pid_t pid;
    char name[MAXPATHLEN + 1];
} pidinfo_t;
typedef struct result_t {
    pidinfo_t pid;
    fdinfo_t fd;
} result_t;

void format_size(uint64_t size, char *result)
{
    uint64_t multiplier;
    int i;

    multiplier = exbibytes;

    for (i = 0 ; i < DIM(sizes) ; i++, multiplier /= 1024) {
        if (size < multiplier)
            continue;
        if (size % multiplier == 0)
            sprintf(result, "%u %s", size / multiplier, sizes[i]);
        else
            sprintf(result, "%.1f %s", (float) size / multiplier, sizes[i]);
        return;
    }

    strcpy(result, "0");
    return;
}

void get_size(char src[32],uint64_t *result)
{
    uint64_t multiplier = 0ULL,value = 0ULL;
    int i = 0, pos = -1;
    if (src == NULL) {
        return;
    }
    char *nptr = src;
    char unit[8]="";
    while (*nptr && isspace ( *nptr ) ) {
        ++ nptr;
    }
    if (nptr == NULL) {
        return;
    }
    value = strtoull(nptr,NULL,10);
    while (*nptr && isspace( *nptr ) == 0 ) {
        ++ nptr;
    }
    nptr ++;
    if (nptr == NULL) {
        return;
    }
    snprintf(unit,sizeof(unit),"%s",nptr);
    multiplier = exbibytes;
    for (i = 0 ; i < DIM(sizes) ; i++) {
        if (strcmp(sizes[i],unit) == 0) {
            pos = i;
            break;
        }
    }
    if (pos < 0) {
        return;
    }
    for (i = 0 ; i < pos ; i++, multiplier /= 1024) {
        if (multiplier <= 0) {
            multiplier = 1;
            break;
        }
    }
    *result = multiplier * value;
    return;
}

char is_numeric(char *str)
{
    while (*str) {
        if(!isdigit(*str))
            return 0;
        str++;
    }
    return 1;
}

static int find_fd_for_pid(const char *bin_name,int *fd_list, int max_fd,pidinfo_t *pidinfo)
{
    DIR *proc;
    struct dirent *direntp;
    struct stat stat_buf;
    char fullpath_dir[MAXPATHLEN + 1];
    char fullpath_exe[MAXPATHLEN + 1];
    char exe[MAXPATHLEN + 1];
    int pid_count=0;
    pid_t pid = 0;
    char link_dest[MAXPATHLEN + 1];
    int count = 0;
    ssize_t len;

    proc=opendir(PROC_PATH);
    if (!proc) {
        printf("opendir");
        return -1;
    }

    while ((direntp = readdir(proc)) != NULL) {
        snprintf(fullpath_dir, MAXPATHLEN, "%s/%s", PROC_PATH, direntp->d_name);
        if (stat(fullpath_dir, &stat_buf) == -1) {
            continue;
        }

        if ((S_ISDIR(stat_buf.st_mode) && is_numeric(direntp->d_name))) {
            snprintf(fullpath_exe, MAXPATHLEN, "%s/exe", fullpath_dir);
            len=readlink(fullpath_exe, exe, MAXPATHLEN);
            if (len != -1)
                exe[len] = 0;
            else {
                // Will be mostly "Permission denied"
                continue;
            }
            char *p = basename(exe);
            if (p==NULL)
                continue;

            if (!strcmp(basename(exe), bin_name)) {
                pid = atol(direntp->d_name);
                pid_count++;
                if(pid_count > 1)
                    break;
            }
        }
    }

    closedir(proc);
    proc = NULL;
    if (pid_count < 1 ) {
        return -1;
    }

    snprintf(fullpath_dir, MAXPATHLEN, "%s/%d/fd", PROC_PATH, pid);

    proc = opendir(fullpath_dir);
    if (!proc) {
        printf("opendir");
        return 0;
    }

    pidinfo->pid = pid;
    snprintf(pidinfo->name,MAXPATHLEN,"%s",bin_name);

    while ((direntp = readdir(proc)) != NULL) {
        snprintf(fullpath_exe, MAXPATHLEN, "%s/%s", fullpath_dir, direntp->d_name);
        if (stat(fullpath_exe, &stat_buf) == -1) {
            continue;
        }
        if(!S_ISREG(stat_buf.st_mode) && !S_ISBLK(stat_buf.st_mode))
            continue;
        len = readlink(fullpath_exe, link_dest, MAXPATHLEN);
        if (len != -1)
            link_dest[len] = 0;
        else
            continue;
        if (stat(link_dest, &stat_buf) == -1)
            continue;
        fd_list[count++] = atoi(direntp->d_name);
        if (count == max_fd)
            break;
    }

    closedir(proc);
    return count;
}

static int get_fdinfo(pid_t pid, int fdnum, fdinfo_t *fd_info)
{
    struct stat stat_buf;
    char fdpath[MAXPATHLEN + 1];
    char line[LINE_LEN];
    FILE *fp;
    int flags;
    struct timezone tz;

    fd_info->num = fdnum;
    fd_info->mode = PM_NONE;

    ssize_t len;
    snprintf(fdpath, MAXPATHLEN, "%s/%d/fd/%d", PROC_PATH, pid, fdnum);

    len=readlink(fdpath, fd_info->name, MAXPATHLEN);
    if (len != -1)
        fd_info->name[len] = 0;
    else {
        return 0;
    }

    if (stat(fd_info->name, &stat_buf) == -1) {
        return 0;
    }

    if (S_ISBLK(stat_buf.st_mode)) {
        int fd;
        fd = open(fd_info->name, O_RDONLY);
        if (fd < 0) {
            return 0;
        }
        if (ioctl(fd, BLKGETSIZE64, &fd_info->size) < 0) {
            close(fd);
            return 0;
        }
        close(fd);
    } else {
        fd_info->size = stat_buf.st_size;
    }

    flags = 0;
    fd_info->pos = 0;
    snprintf(fdpath, MAXPATHLEN, "%s/%d/fdinfo/%d", PROC_PATH, pid, fdnum);
    fp = fopen(fdpath, "rt");
    gettimeofday(&fd_info->tv, &tz);
    if (!fp) {
        return 0;
    }
    while (fgets(line, LINE_LEN - 1, fp) != NULL) {
        if (!strncmp(line, "pos:", 4))
            fd_info->pos = atoll(line + 5);
        if (!strncmp(line, "flags:", 6))
            flags = atoll(line + 7);
    }
    if ((flags & O_ACCMODE) == O_RDONLY)
        fd_info->mode = PM_READ;
    if ((flags & O_ACCMODE) == O_WRONLY)
        fd_info->mode = PM_WRITE;
    if ((flags & O_ACCMODE) == O_RDWR)
        fd_info->mode = PM_READWRITE;
    fclose(fp);
    return 1;
}

int monitor_processes(const char *cmd,char *pos,char *tsize)
{
    int pid_count=0 ,fd_count=0, result_count=0;
    int i,j;
    pidinfo_t pidinfo_list[1];
    fdinfo_t fdinfo;
    fdinfo_t biggest_fd;
    int fdnum_list[MAX_FD_PER_PID];
    off_t max_size;
    char fsize[64];
    char fpos[64];
    char ftroughput[64];
    float perc;
    signed char still_there;
    signed char search_all = 1;
    static signed char first_pass = 1;

    first_pass = 0;
    search_all = 0;
    pidinfo_t pidinfo;
    memset(&pidinfo,0,sizeof(pidinfo_t));
    fd_count = find_fd_for_pid(cmd,fdnum_list, MAX_FD_PER_PID,&pidinfo);

    max_size = 0;
    //printf("\n fd_count[%d]\n",fd_count);
    if (fd_count < 1) {
        return -1;
    }
    for (j = 0 ; j < fd_count ; j++) {
        get_fdinfo(pidinfo.pid, fdnum_list[j], &fdinfo);
        if (fdinfo.size > max_size) {
            biggest_fd = fdinfo;
            max_size = fdinfo.size;
        }
    }

    if (!max_size) { // nothing found
        return -1;
    }
    format_size(biggest_fd.pos, fpos);
    format_size(biggest_fd.size, fsize);
    perc = ((double)100 / (double)biggest_fd.size) * (double)biggest_fd.pos;

#if 0
    printf("[%5d] %s %s\n\t%.1f%% (%s / %s)",
        pidinfo.pid,
        pidinfo.name,
        biggest_fd.name,
        perc,
        fpos,
        fsize);
#endif
    snprintf(pos,31,"%s",fpos);
    snprintf(tsize,31,"%s",fsize);
    return (int)perc;
}

int setDiskCfgInfo(const char *file,diskcfginfo_t *cfgInfo)
{
    if (file == NULL || cfgInfo == NULL) {
        return -1;
    }
    int fd = open(file,O_RDWR | O_CREAT | O_TRUNC,0640);
    if (fd < 0) {
        return -1;
    }
    write(fd,cfgInfo,sizeof(diskcfginfo_t));
    close(fd);
    return 0;
}

int getDiskCfgInfo(const char *file,diskcfginfo_t *cfgInfo)
{
    if (file == NULL || cfgInfo == NULL) {
        return -1;
    }
    int fd = open(file,O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    read(fd,cfgInfo,sizeof(diskcfginfo_t));
    close(fd);
    return 0;
}
