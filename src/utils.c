// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
#include "utils.h"

#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>

#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>

// Currently i am mimicking old incrond behaviour - next cycle this will change to cron manner
int check_spool_dir(const char* spool_dir)
{
    int errsv = 0;

    if(access(spool_dir, X_OK) == -1) {
        errsv = errno;
        if(errsv != ENOENT) {
            syslog(LOG_CRIT, "access spool dir %s failed with %d : %s", spool_dir, errsv, strerror(errsv));
            goto fail;
        }

        if(mkdir(spool_dir, 0755) == -1)
        {
            errsv = errno;
            syslog(LOG_CRIT, "creating spool dir %s failed with %d : %s", spool_dir, errsv, strerror(errsv));
            goto fail;
        }

        syslog(LOG_INFO, "created spool dir %s", spool_dir);
    }

    return open(spool_dir, O_DIRECTORY);

    fail:
    errno = errsv;
    return -1;
}
