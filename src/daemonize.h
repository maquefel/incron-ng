// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
#ifndef __DAEMONIZE_H__
#define __DAEMONIZE_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#define DM_NO_MASK        01
#define DM_NO_STD_CLOSE   04

static inline int daemonize(const char* dir, int flags) {
    pid_t pid = 0;
    int ret = 0;
    int errsv = 0;

    pid = fork();
    errsv = errno;

    if(pid < 0) {
        errno = errsv;
        return -1;
    }

    if(pid > 0)
        _exit(EXIT_SUCCESS);

    ret = setsid();

    if(ret < 0)
        return -1;

    pid = fork();

    if(pid < 0)
        return -1;

    if(pid > 0)
        _exit(EXIT_SUCCESS);

    if(dir != NULL) {
        ret = chdir(dir);
        if(ret == -1)
            /** not such a big deal but report it */
            syslog(LOG_WARNING, "failed chdir to %s with [%d] : %s", dir, errno, strerror(errno));
    }

    if(!(flags & DM_NO_STD_CLOSE)) {
        close(STDIN_FILENO);
        int fd = open("/dev/null", O_RDWR);

        if(fd != 0)
            return -1;

        dup2(STDIN_FILENO, STDOUT_FILENO);
        dup2(STDIN_FILENO, STDERR_FILENO);
    }

    return 0;
}

static inline pid_t read_pid_file_(int fd)
{
    pid_t pid = -1;
    char buffer[16] = {0};

    if(fd > 0) {
        int ret = read(fd, buffer, sizeof(buffer) - 1);
        int errsv = errno;

        if(ret > 0) {
            pid = strtol(buffer, NULL, 10);
            errsv = errno;
        }


        if(pid == 0 || errsv == ERANGE)
            pid = -1;

        close(fd);
    }

    return pid;
};

static inline pid_t read_pid_file(const char* pidFile)
{
    int pid_fd = 0;
    pid_fd = open(pidFile, O_RDONLY);
    return read_pid_file_(pid_fd);
}

static inline pid_t read_pid_fileat(int dirfd, const char* pidFile)
{
    int pid_fd = 0;
    pid_fd = openat(dirfd, pidFile, O_RDONLY);
    return read_pid_file_(pid_fd);
}

static inline pid_t create_pid_file_(int fd)
{
    int ret = 0;
    int errsv = 0;
    char buffer[16] = {0};
    pid_t pid = 0;

    pid = getpid();
    ret = sprintf(buffer, "%d\n", pid);
    ret = write(fd, buffer, ret);
    errsv = errno;

    close(fd);

    if(ret == -1)
        goto fail;

    return pid;

    fail:
    errno = errsv;
    return -1;
}

static inline pid_t create_pid_file(const char* pidFile)
{
    int pid_fd = 0;
    pid_fd = open(pidFile, O_CREAT | O_WRONLY | O_EXCL, 0644);
    return create_pid_file_(pid_fd);
}

static inline pid_t create_pid_fileat(int dirfd, const char* pidFile)
{
    int pid_fd = 0;
    pid_fd = openat(dirfd, pidFile, O_CREAT | O_WRONLY | O_EXCL, 0644);
    return create_pid_file_(pid_fd);
}

static inline int get_filename_from_fd(int fd, char* buffer)
{
    int errsv = EINVAL;
    int ret = 0;
    ssize_t nbytes, size;

    size_t len = snprintf(NULL, 0, "/proc/%d/fd/%d", getpid(), fd);
    char proc_filepath[len + 1];

    sprintf(proc_filepath, "/proc/%d/fd/%d", getpid(), fd);

    struct stat sb;

    ret = lstat(proc_filepath, &sb);
    if(ret == -1) {
        errsv = errno;
        goto fail;
    }

    size = sb.st_size;

    if(size == 0)
        size = PATH_MAX;

    if(buffer == 0)
        return size;

    nbytes = readlink(proc_filepath, buffer, size + 1);
    if(nbytes == -1) {
        errsv = errno;
        goto fail;
    }

    buffer[nbytes] = '\0';

    return nbytes;
    fail:
    errno = errsv;
    return -1;
}

#endif
