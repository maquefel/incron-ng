#!/usr/bin/env bats
# SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
# SPDX-License-Identifier: CC0-1.0

INCROND=../incrond

setup() {
    SKIP_TEST=1
    if [[ $EUID -eq 0 ]]; then
        ${INCROND} -f ${CURDIR}/etc/incron.conf
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

@test "user_hook" {
    if [[ $SKIP_TEST -ne 0 ]]; then
        skip "This test needs to be run as root!"
    fi

    pid=$(ps -e | grep incrond)
}
