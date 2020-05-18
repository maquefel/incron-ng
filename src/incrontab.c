// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
/** @file incrontab.c
 *
 * @brief Main file for the incrontab edit tool
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
#include <stdbool.h>
#include <pwd.h>
#include <fcntl.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/signalfd.h>
#include <sys/wait.h>

#include <linux/limits.h>

#include "incrond.h"
#include "incrond-parse-tabs.h"
#include "incrond-dispatch.h"
#include "incrond-config.h"
#include "incrond-exec.h"
#include "utils.h"
#include "daemonize.h"

#define INCRON_ALT_EDITOR "/etc/alternatives/editor" ///> Alternative editor
#define INCRON_DEFAULT_EDITOR "vim" ///> Default (hard-wired) editor

/** pid file */
int lockfile_dir_fd;
char lockfile_name[NAME_MAX];

/** config file */
char *configFile = INCROND_CONFIG_FILE;

/** editor */
char editor_name[NAME_MAX];

int system_table_dir_fd;
int user_table_dir_fd;

static struct option long_options[] =
{
    {"help",            no_argument,        0,                  'h'},
    {"list",            no_argument,        0,                  'l'},
    {"remove",          no_argument,        0,                  'r'},
    {"edit",            no_argument,        0,                  'e'},
    {"types",           no_argument,        0,                  't'},
    {"reload",          no_argument,        0,                  'd'},
    {"version",         no_argument,        0,                  'V'},
    {"user",            required_argument,  0,                  'u'},
    {"config",          required_argument,  0,                  'f'},
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
            "incrontab - inotify cron table manipulator\n" \
            "(c) Nikita Shubin 2019\n" \
            "(c) Andreas Altair Redmer, 2014, 2015\n" \
            "(c) Lukas Jelinek, 2006, 2007, 2008 \n" \
            "\n" \
            "usage: incrontab [<options>] <operation>\n" \
            "       incrontab [<options>] <FILE-TO-IMPORT>\n" \
            "\n" \
            "<operation> may be one of the following:\n" \
            "  -?, --about                    gives short information about program\n" \
            "  -h, --help                     prints this help text\n" \
            "  -l, --list                     lists user table\n" \
            "  -r, --remove                   removes user table\n" \
            "  -e, --edit                     provides editing user table\n" \
            "  -t, --types                    list supported event types\n" \
            "  -d, --reload                   request incrond to reload user table\n" \
            "  -V, --version                  prints program version\n"
            "\n" \
            "\n" \
            "These options may be used: \n" \
            "  -u <USER>, --user=<USER>     overrides current user (requires root privileges)\n" \
            "  -f <FILE>, --config=<FILE>   overrides default configuration file  (requires root privileges)\n" \
        );
    }
}

// Editor selecting algorithm:
// 1. Check EDITOR environment variable
// 2. Check VISUAL environment variable
// 3. Try to get from configuration
// 4. Check presence of /etc/alternatives/editor
// 5. Use hard-wired editor
static const char* editor()
{
    char *e = INCRON_DEFAULT_EDITOR;
    e = getenv("EDITOR");
    if(e != 0) goto out;

    e = getenv("VISUAL");
    if(e != 0) goto out;

    if(strlen(editor_name) != 0)
    {
        e = editor_name;
        goto out;
    }

    // /etc/alternatives/editor
    if(access(INCRON_ALT_EDITOR, X_OK) == 0)
        e = INCRON_ALT_EDITOR;

    out:
    return e;
}

static int copy_files(int fd_in, int fd_out)
{
    int fd[2];
    int ps = sysconf(_SC_PAGE_SIZE);
    int len = -1;
    loff_t in_off = 0;
    loff_t out_off = 0;
    int ret = pipe2(fd, O_CLOEXEC);
    if(ret == -1)
        goto fail_no_close;

    do {
        len = splice(fd_in, &in_off, fd[1], 0, ps, 0);
        if(len == -1) goto fail;
        len = splice(fd[0], 0, fd_out, &out_off, len, 0);
        if(len == -1) goto fail;
    } while(len == ps);

    fail:
    close(fd[0]);
    close(fd[1]);
    fail_no_close:
    return len;
}

static int copy_to_stdout(int fd_in)
{
    int fd[2];
    int ps = sysconf(_SC_PAGE_SIZE);
    int len = -1;
    int ret = pipe2(fd, O_CLOEXEC);
    if(ret == -1)
        goto fail_no_close;

    do {
        char buffer[ps];
        len = read(fd_in, buffer, ps);
        if(len == -1) goto fail;
        len = write(STDOUT_FILENO, buffer, len);
        if(len == -1) goto fail;
    } while(len == ps);

    fail:
    close(fd[0]);
    close(fd[1]);
    fail_no_close:
    return len;
}

static inline time_t get_mtime(int fd)
{
    struct stat st;
    int ret = fstat(fd, &st);
    if(ret == -1) {
        fprintf(stderr, "cannot stat temporary file: %s\n", strerror(errno));
        return (time_t)0;
    }

    return st.st_mtime;
}

static int list_table(struct passwd* pwd)
{
    int ret = -1;
    int errsv = 0;

    int user_fd = openat(user_table_dir_fd, pwd->pw_name, O_RDWR);

    if(user_fd == -1) {
        errsv = errno;

        if(errsv == ENOENT) {
            fprintf(stderr, "no table for %s\n", pwd->pw_name);
            ret = 0; /** not really fail */
            goto fail;
        }

        fprintf(stderr, "cannot read table for '%s': %s\n", pwd->pw_name, strerror(errno));
        goto fail;
    }


    ret = copy_to_stdout(user_fd);
    errsv = errno;

    close(user_fd);

    fail:
    errno = errsv;
    return ret;
}

/** taken from cronie/src/misc.c */
static uid_t save_euid;
static gid_t save_egid;

static inline int swap_uids(void) {
    save_egid = getegid();
    save_euid = geteuid();
    return ((setegid(getgid()) || seteuid(getuid()))? -1 : 0);
}

static inline int swap_uids_back(void) {
    return ((setegid(save_egid) || seteuid(save_euid)) ? -1 : 0);
}

static int edit_table(struct passwd* pwd)
{
    static char file_template[] = "/tmp/incron.table-XXXXXX";
    static char fname[NAME_MAX];

    int errsv = 0;
    int ret = -1;
    time_t mt = (time_t) 0;

    uid_t uid = pwd->pw_uid;
    uid_t gid = pwd->pw_gid;

    /* open user file or open null if file not exists */
    int user_fd = openat(user_table_dir_fd, pwd->pw_name, O_RDONLY);
    if(user_fd == -1) {
        errsv = errno;
        if(errsv != ENOENT) {
            fprintf(stderr, "cannot read old table for '%s': %s\n", pwd->pw_name, strerror(errno));
            user_fd = openat(user_table_dir_fd, pwd->pw_name, O_RDONLY);
            goto fail;
        }

        user_fd = open("/dev/null", O_RDONLY);
        if(user_fd == -1) {
            errsv = errno;
            fprintf(stderr, "cannot get empty table for '%s': %s\n", pwd->pw_name, strerror(errsv));
            goto fail;
        }
    }

    /* Turn off signals. */
    (void)signal(SIGHUP, SIG_IGN);
    (void)signal(SIGINT, SIG_IGN);
    (void)signal(SIGQUIT, SIG_IGN);

    /* drop effective gid uid */
    if (swap_uids() != 0) {
        errsv = errno;
        fprintf(stderr, "cannot change effective UID/GID for user '%s': %s\n", pwd->pw_name, strerror(errsv));
        goto fail;
    }

    /* create temporary file */
    strncpy(fname, file_template, strlen(file_template));

    int tmp_fd = mkstemp(fname);
    if(tmp_fd == -1) {
        errsv = errno;
        goto fail_close_user_fd;
    }

    /* get effective uid gid back */
    if (swap_uids() != 0) {
        errsv = errno;
        fprintf(stderr, "cannot change effective UID/GID for user '%s': %s\n", pwd->pw_name, strerror(errsv));
        goto fail;
    }

    /* copy user to temporary */
    ret = copy_files(user_fd, tmp_fd);
    if(ret == -1) {
        errsv = errno;
        goto fail_close_tmp_fd;
    }

    close(user_fd);

    /* drop effective gid uid */
    if (swap_uids() != 0) {
        errsv = errno;
        fprintf(stderr, "cannot change effective UID/GID for user '%s': %s\n", pwd->pw_name, strerror(errsv));
        goto fail;
    }

    mt = get_mtime(tmp_fd);

    {
        const char* edt = editor();
        // editor filename
        size_t len = snprintf(0, 0, "%s %s", edt, fname);
        char shell_arg[++len];
        len = snprintf(shell_arg, len, "%s %s", edt, fname);

        char* argv[] = {
            "/bin/bash",
            "-c",
            shell_arg,
            0
        };

        pid_t pid = fork();
        if(pid == 0)
        {
            /* set users gid uid */
            if (setgid(gid) == -1 || setuid(uid) == -1) {
                fprintf(stderr, "failed setting user gid/uid after fork\n");
                exit(EXIT_FAILURE);
            }

            execvpe("/bin/bash", argv, environ);
            exit(EXIT_FAILURE); // we should not be here
        }

        if(pid == -1)
        {
            ret = -1;
            perror("cannot start editor");
            errsv = errno;
            goto fail_close_tmp_fd;
        }

        int status;
        ret = waitpid(pid, &status, 0);
        if(ret == -1) {
            perror("error while waiting for editor");
            goto fail_close_tmp_fd;
        }

        if (!(WIFEXITED(status)) || WEXITSTATUS(status) != 0) {
            ret = -1;
            perror("editor finished with error");
            goto fail_close_tmp_fd;
        }
    }

    /* turn back signals */
    (void) signal(SIGHUP, SIG_DFL);
    (void) signal(SIGINT, SIG_DFL);
    (void) signal(SIGQUIT, SIG_DFL);

    if(mt == get_mtime(tmp_fd))
    {
        fprintf(stderr, "table unchanged\n");
        ret = 0;
        goto cleanup;
    }

    /* get effective gid uid back */
    if (swap_uids() != 0) {
        errsv = errno;
        fprintf(stderr, "cannot change effective UID/GID for user '%s': %s\n", pwd->pw_name, strerror(errsv));
        goto fail;
    }

    /** reopen */
    user_fd = openat(user_table_dir_fd, pwd->pw_name, O_WRONLY | O_CREAT, 0600);
    if(user_fd == -1)
    {
        errsv = errno;
        fprintf(stderr, "failed reopening userfile with %d : %s\n", errsv, strerror(errsv));
        goto cleanup;
    }

    ret = copy_files(tmp_fd, user_fd);
    if(ret == -1)
    {
        fprintf(stderr, "cannot move temporary table: %s\n", strerror(errno));
        goto cleanup;
    }

    fprintf(stderr, "table updated\n");

    cleanup:
    fail_close_tmp_fd:
    close(tmp_fd);
    fail_close_user_fd:
    close(user_fd);
    fail:
    errno = errsv;
    return ret;
};

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
    int c = -1;
    int option_index = 0;

    int errsv = 0;
    int ret = 0;

    int list_flag = 0;
    int remove_flag = 0;
    int edit_flag = 0;
    int types_flag = 0;
    int reload_flag = 0;

    int explicit_config = 0;

    int login_name_max = sysconf(_SC_LOGIN_NAME_MAX);
    char user[login_name_max];
    user[0] = '\0';

    size_t bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
    char buffer[bufsize];
    struct passwd pwd;
    struct passwd *result;


    while((c = getopt_long(argc, argv, "?hlretdVu:f:", long_options, &option_index)) != -1) {
        switch(c) {
            case '?':
                /** @todo similiar behavior to original - only version and authors */
            case 'h':
                PrintUsage(argc, argv);
                exit(EXIT_SUCCESS);
                break;
            case 'l':
                list_flag = 1;
                break;
            case 'r':
                remove_flag = 1;
                break;
            case 'e':
                edit_flag = 1;
                break;
            case 't':
                types_flag = 1;
                break;
            case 'd':
                reload_flag = 1;
                break;
            case 'V' :
                printf("%s", daemon_version());
                exit(EXIT_SUCCESS);
                break;
            case 'u':
                strncpy(user, optarg, sizeof(user));
                break;
            case 'f':
                configFile = optarg;
                explicit_config = 1;
                break;
            default:
                break;
        }
    }

    ret = loadConfigFile(configFile);
    errsv = errno;
    if(explicit_config && ret == -1) {
        syslog(LOG_CRIT, "loading config %s failed with %d:%s", configFile, errsv, strerror(errsv));
        goto quit;
    }

    loadUsers();

    if(strlen(user) == 0) { // get current user then
        ret = getpwuid_r(getuid(), &pwd, buffer, bufsize, &result);
        strncpy(user, pwd.pw_name, sizeof(user));
    } else {
        ret = getpwnam_r(user, &pwd, buffer, bufsize, &result);
    }
    if(result == 0) {
        if(ret == 0) { // NOT FOUND
            fprintf(stderr, "uid %d not found \n", getuid());
        } else  {
            errsv = ret;
            perror("getpwuid_r");
        }

        goto quit;
    }

    /** check if user capable */
    if(!userAllowed(user)) {
        printf("user '%s' is not allowed to use incron\n", user);
        goto quit;
    }

    /** spool dir checking is the last thing */
    user_table_dir_fd = check_spool_dir(user_table_dir);

    if(list_flag) {
        if(user_table_dir_fd == -1) {
            printf("no table for %s\n", user);
            goto quit;
        }

        ret = list_table(&pwd);
        goto quit;
    }

    if(remove_flag) {
        printf("removing table for user '%s'\n", user);
        ret = unlinkat(user_table_dir_fd, user, 0);
        if(ret == -1) {
            printf("table for user '%s' does not exist\n", user);
            goto quit;
        }
    }

    if(edit_flag) {
        ret = edit_table(&pwd);
        goto quit;
    }

    if(types_flag) {
        printf("%s\n", print_text_events(IN_ALL_EVENTS));
        goto quit;
    }

    if(reload_flag) {

    }

    quit:
    return ret;
}
