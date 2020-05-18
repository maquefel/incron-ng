// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
/** @file incrond.c
*
* @brief Main file for the incrond daemon
*
* @par
*/
#include <stdlib.h>
#include <syslog.h>
#include <getopt.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/signalfd.h>

#include "incrond.h"
#include "daemonize.h"
#include "utils.h"
#include "incrond-loop.h"
#include "incrond-config.h"
#include "incrond-parse-tabs.h"

static int verbose_flag = 0;
static int no_daemon_flag = 0;
static int kill_flag = 0;
static int hup_flag = 0;

/** signal fd */
int sigfd;

/** /dev/null fd*/
int devnullfd;

/** logging */
#ifdef DEBUG
int log_level = LOG_DEBUG;
#else
int log_level = LOG_INFO;
#endif

/** pid file */
static char *pidFile;
int lockfile_dir_fd;
char lockfile_name[NAME_MAX];

/** config file */
char *configFile = INCROND_CONFIG_FILE;

/** editor */
char editor_name[NAME_MAX];

static struct option long_options[] =
{
    {"verbose",         no_argument,        &verbose_flag,      'v'},
    {"version",         no_argument,        0,                  'V'},
    {"log-level",       required_argument,  0,                  'l'},
    {"foreground",      no_argument,        &no_daemon_flag,    'n'},
    {"kill",            no_argument,        &kill_flag,         'k'},
    {"hup",             no_argument,        &hup_flag,          'H'},
    {"pid",             required_argument,  0,                  'p'},
    {"config",          required_argument,  0,                  'f'},
    {"help",            no_argument,        0,                  'h'},
    {0, 0, 0, 0}
};

/**************************************************************************
*    Function: Daemon version
*    Description:
*        Output the nul terminated string with daemon version.
*    Params:
*    Returns:
*        returns nul terminated string with daemon version
**************************************************************************/
char* daemon_version()
{
    static char version[31] = {0};
    snprintf(version, sizeof(version), "%d.%d.%d\n", MAJOR, MINOR, VERSION_PATCH);
    return version;
}

/**************************************************************************
*    Function: Print Usage
*    Description:
*        Output the command-line options for this daemon.
*    Params:
*        @argc - Standard argument count
*        @argv - Standard argument array
*    Returns:
*        returns void always
**************************************************************************/
void PrintUsage(int argc, char *argv[]) {
    if(argc >=1) {
        printf(
        "incrond - inotify cron daemon\n" \
        "(c) Nikita Shubin 2019\n" \
        "(c) Andreas Altair Redmer, 2014, 2015\n" \
        "(c) Lukas Jelinek, 2006, 2007, 2008 \n" \
        "\n" \
        "usage: incrond [<options>]\n" \
        "<operation> may be one of the following:\n" \
        "These options may be used:\n" \
        "-?, --about                    gives short information about program\n" \
        "-h, --help                     prints this help text\n" \
        "-n, --foreground               runs on foreground (no daemonizing)\n" \
        "-k, --kill                     terminates running instance of incrond\n" \
        "-f <FILE>, --config=<FILE>     overrides default configuration file  (requires root privileges)\n" \
        "-V, --version                  prints program version\n"
        "-v, --verbose                  be more verbose\n" \
        "-l, --log-level                set log level[default=LOG_INFO]\n" \
        "-H, --hup                      send daemon signal to reload configuration\n"
        );
    }
}

/**************************************************************************
*    Function: main
*    Description:
*        The c standard 'main' entry point function.
*    Params:
*        @argc - count of command line arguments given on command line
*        @argv - array of arguments given on command line
*    Returns:
*        returns integer which is passed back to the parent process
**************************************************************************/
int main(int argc, char ** argv)
{
    int daemon_flag = 1; //daemonizing by default
    int c = -1;
    int option_index = 0;

    int errsv = 0;
    int ret = 0;

    while((c = getopt_long(argc, argv, "Vvl:nkHp:f:h", long_options, &option_index)) != -1) {
                switch(c) {
            case 'v' :
                printf("%s", daemon_version());
                exit(EXIT_SUCCESS);
                break;
            case 'V' :
                verbose_flag = 1;
                break;
            case 'l':
                log_level = strtol(optarg, 0, 10);
                break;
            case 'n' :
                printf("Not daemonizing!\n");
                daemon_flag = 0;
                break;
            case 'k' :
                kill_flag = 1;
                break;
            case 'H' :
                hup_flag = 1;
                break;
            case 'p' :
                pidFile = optarg;
                break;
            case 'f':
                configFile = optarg;
                printf("Using config file: %s\n", configFile);
                break;
            case 'h':
                PrintUsage(argc, argv);
                exit(EXIT_SUCCESS);
                break;
            default:
                break;
        }
    }

    ret = loadConfigFile(configFile);
    errsv = errno;
    if(ret == -1) {
        syslog(LOG_CRIT, "loading config %s failed with %d:%s", configFile, errsv, strerror(errsv));
        goto quit;
    }

    if(pidFile == 0)
        pidFile = lockfile_name;

    pid_t pid = read_pid_fileat(lockfile_dir_fd, pidFile);
    errsv = errno;

    if(pid > 0) {
        ret = kill(pid, 0);
        if(ret == -1) {
            size_t path_len = get_filename_from_fd(lockfile_dir_fd, 0);
            char path[path_len];
            get_filename_from_fd(lockfile_dir_fd, path);

            fprintf(stderr, "%s : %s/%s pid file exists, but the process doesn't!\n", PACKAGE_NAME, path, pidFile);

            if(kill_flag || hup_flag)
                goto quit;

            unlinkat(lockfile_dir_fd, pidFile, 0);
        } else {
            /** check if -k (kill) passed*/
            if(kill_flag)
            {
                kill(pid, SIGTERM);
                goto quit;
            }

            /** check if -h (hup) passed*/
            if(hup_flag)
            {
                kill(pid, SIGHUP);
                goto quit;
            }
        }
    }

    if(daemon_flag) {
        daemonize("/", 0);
    } else
        openlog(PACKAGE_NAME, LOG_PERROR, LOG_DAEMON);

    pid = create_pid_fileat(lockfile_dir_fd, pidFile);
    errsv = errno;
    if(pid == -1)
    {
        size_t path_len = get_filename_from_fd(lockfile_dir_fd, 0);
        char path[path_len];
        get_filename_from_fd(lockfile_dir_fd, path);

        syslog(LOG_ERR, "Failed creating pid file %s/%s (%s).", path, pidFile, strerror(errsv));
    }

    /** setup signals */
    sigset_t mask;
    sigemptyset(&mask);
    //sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGPIPE);
    sigaddset(&mask, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        syslog(LOG_ERR, "Could not register signal handlers (%s).", strerror(errno));
        goto unlink_pid;
    }

    /** get sigfd */
    sigfd = signalfd(-1, &mask, SFD_CLOEXEC);

    /** set log level */
    setlogmask(LOG_UPTO(log_level));

    ret = loadSystemTabs(system_table_dir_fd);
    if(ret == -1) {
        errsv = errno;
        syslog(LOG_CRIT, "loading system tabs failed with %d:%s", errsv, strerror(errsv));
        goto unlink_pid;
    }

    loadUsers();

    user_table_dir_fd = check_spool_dir(user_table_dir);
    if(user_table_dir_fd == -1) {
        errsv = errno;
        syslog(LOG_CRIT, "loading users tabs at %s failed with %d:%s", user_table_dir, errsv, strerror(errsv));
    } else {
        ret = loadUserTabs(user_table_dir_fd);
        if(ret == -1) {
            errsv = errno;
            syslog(LOG_CRIT, "loading users tabs at %s failed with %d:%s", user_table_dir, errsv, strerror(errsv));
        }
    }

    ret = loop(sigfd);

    unlink_pid:
    unlinkat(lockfile_dir_fd, pidFile, 0);

    quit:
    return ret;
}
