#!/usr/bin/env bats
# SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
# SPDX-License-Identifier: CC0-1.0

INCROND=../incrond
CFG=etc/incron.conf
[ -z "$TEST_USER" ] && TEST_USER=user_hook_test

setup() {
    SKIP_TEST=1
    if [[ $EUID -eq 0 ]]; then
        useradd $TEST_USER
        coproc incrond { ${INCROND} -f ${CFG} -n; }
        SAVED_PID=$incrond_PID
        sleep 0.5
        SKIP_TEST=0
    fi
}

teardown() {
    if [[ $SKIP_TEST -ne 1 ]]; then
        timeout 1 ${INCROND} -f ${CFG} -k
        wait $SAVED_PID || true
        userdel $TEST_USER
    fi
}

wait_for_file() {
    cnt=$(($2 - 1))
    while ! test -f "$1"; do
        [ $cnt -eq 0 ] && return 1
        cnt="$(($cnt - 1))"
        sleep 0.1
    done

    return 0
}

compare_files() {
    f1=$(sed -n 1p $1 | awk -v f="$3" '{ print $f }')
    f2=$(sed -n 1p $2 | awk -v f="$3" '{ print $f }')

    [ x"${f1}" == "x${f2}" ] && return 0

    return 1
}

@test "user_hook" {
    if [[ $SKIP_TEST -ne 0 ]]; then
        skip "This test needs to be run as root!"
    fi

    find ../ -type f \( -name "*.gcda" -o -name "*.gcno" \) -exec chown $TEST_USER:$TEST_USER {} \;

    TAB_NAME=var/spool/
    LOG_NAME=/tmp/watch_user_exec.log

    rm -rf ${LOG_NAME}

    echo 1 > tmp/watch_user_exec
    cat tmp/watch_user_exec > /dev/null

    wait_for_file ${LOG_NAME} 10

    [ $? -eq 0 ]

    user=$(cat ${LOG_NAME} | awk '{ print $1 }')
    home_dir=$(cat ${LOG_NAME} | awk '{ print $2 }')

    echo $user
    echo $TEST_USER
    [ "$user" == "$TEST_USER" ]

    user_uid=$(id -u $user)

    file_uid=$(stat -c '%u' ${LOG_NAME})

    [ $user_uid -eq $file_uid ]

    rm -rf ${LOG_NAME}
}
