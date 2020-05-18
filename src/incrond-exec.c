// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
#include "incrond-exec.h"

#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>

int exec_start(const char* exec_file, char *const argv[], char *const envp[])
{
    if(access(exec_file, X_OK) == -1)
        return -1;

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /** restore original mask */
    sigset_t mask;
    if (sigprocmask(0, NULL, &mask) == -1)
        return -1;

    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
        return -1;

    return execvpe(exec_file, argv, envp);
}

int exec_stop(pid_t pid)
{
    int errsv = 0;

    if(pid == 0) {
        errsv = EINVAL;
        goto fail;
    }

    return kill(pid, SIGTERM);

    fail:
    errno = errsv;
    return -1;
}

int exec_kill(pid_t pid)
{
    int errsv = 0;

    if(pid == 0) {
        errsv = EINVAL;
        goto fail;
    }

    return kill(pid, SIGKILL);

    fail:
    errno = errsv;
    return -1;
}
