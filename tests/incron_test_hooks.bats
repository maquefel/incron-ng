#!/usr/bin/env bats
# SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
# SPDX-License-Identifier: CC0-1.0

INCROND=../incrond
CFG=etc/incron.conf

setup() {
    coproc incrond { ${INCROND} -f ${CFG} -n; }
    SAVED_PID=$incrond_PID
    sleep 0.5
}

teardown() {
    timeout 1 ${INCROND} -f ${CFG} -k
    wait $SAVED_PID || true
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

@test "hook_in_access" {
    TAB_NAME=etc/incron.d/hook_in_access
    LOG_NAME=log/IN_ACCESS.log

    # zero read doesn't generate IN_ACCESS
    echo 1 > tmp/watch_IN_ACCESS
    cat tmp/watch_IN_ACCESS > /dev/null

    wait_for_file ${LOG_NAME} 10

    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 1
    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 2
    [ $? -eq 0 ]
}

@test "hook_in_attrib" {
    TAB_NAME=etc/incron.d/hook_in_attrib
    LOG_NAME=log/IN_ATTRIB.log

    touch tmp/watch_IN_ATTRIB

    wait_for_file ${LOG_NAME} 10

    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 1
    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 2
    [ $? -eq 0 ]
}

@test "hook_in_close_write" {
    TAB_NAME=etc/incron.d/hook_in_close_write
    LOG_NAME=log/IN_CLOSE_WRITE.log

    echo 1 > tmp/watch_IN_CLOSE_WRITE

    wait_for_file ${LOG_NAME} 10

    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 1
    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 2
    [ $? -eq 0 ]
}

@test "hook_in_close_no_write" {
    TAB_NAME=etc/incron.d/hook_in_close_nowrite
    LOG_NAME=log/IN_CLOSE_NOWRITE.log

    echo 1 > tmp/watch_IN_CLOSE_NOWRITE
    cat tmp/watch_IN_CLOSE_NOWRITE > /dev/null

    wait_for_file ${LOG_NAME} 10

    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 1
    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 2
    [ $? -eq 0 ]
}

@test "hook_in_create" {
    TAB_NAME=etc/incron.d/hook_in_create
    LOG_NAME=log/IN_CREATE.log

    rm -rf tmp/watch_IN_CREATE/test
    touch tmp/watch_IN_CREATE/test

    wait_for_file ${LOG_NAME} 10

    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 1
    [ $? -eq 0 ]

    f2=$(sed -n 1p ${LOG_NAME} | awk '{ print $2 }')
    [ "$f2" == "test" ]

    f3=$(sed -n 1p ${LOG_NAME} | awk '{ print $3 }')
    [ "$f3" == "IN_CREATE" ]
}

@test "hook_in_delete" {
    TAB_NAME=etc/incron.d/hook_in_delete
    LOG_NAME=log/IN_DELETE.log

    touch tmp/watch_IN_DELETE/test
    rm -rf tmp/watch_IN_DELETE/test

    wait_for_file ${LOG_NAME} 10

    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 1
    [ $? -eq 0 ]

    f2=$(sed -n 1p ${LOG_NAME} | awk '{ print $2 }')
    [ "$f2" == "test" ]

    f3=$(sed -n 1p ${LOG_NAME} | awk '{ print $3 }')
    [ "$f3" == "IN_DELETE" ]
}

@test "hook_in_delete_self" {
    TAB_NAME=etc/incron.d/hook_in_delete_self
    LOG_NAME=log/IN_DELETE_SELF.log

    rm -rf tmp/watch_IN_DELETE_SELF

    wait_for_file ${LOG_NAME} 10

    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 1
    [ $? -eq 0 ]

    f2=$(sed -n 1p ${LOG_NAME} | awk '{ print $2 }')
    [ "$f2" == "IN_DELETE_SELF" ] || [ "$f2" == "IN_IGNORED" ]

    f3=$(sed -n 2p ${LOG_NAME} | awk '{ print $2 }')
    [ "$f3" == "IN_IGNORED" ] || [ "$f3" == "IN_DELETE_SELF" ]
}

@test "hook_in_modify" {
    # It's pretty the same as IN_CLOSE_WRITE
    TAB_NAME=etc/incron.d/hook_in_modify
    LOG_NAME=log/IN_MODIFY.log

    echo 1 > tmp/watch_IN_MODIFY

    wait_for_file ${LOG_NAME} 10

    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 1
    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 2
    [ $? -eq 0 ]
}

@test "hook_in_move_self" {
    TAB_NAME=etc/incron.d/hook_in_move_self
    LOG_NAME=log/IN_MOVE_SELF.log

    mv tmp/watch_IN_MOVE_SELF tmp/watch_IN_MOVE_SELF_2

    wait_for_file ${LOG_NAME} 10

    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 1
    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 2
    [ $? -eq 0 ]
}

@test "hook_in_moved_from" {
    TAB_NAME=etc/incron.d/hook_in_moved_from
    LOG_NAME=log/IN_MOVED_FROM.log

    touch tmp/watch_IN_MOVED_FROM/moved_filename

    mv tmp/watch_IN_MOVED_FROM/moved_filename tmp/moved_filename_new

    wait_for_file ${LOG_NAME} 10

    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 1
    [ $? -eq 0 ]

    fn=$(sed -n 1p ${LOG_NAME} | awk '{ print $2 }')
    [ "$fn" == "moved_filename" ]

    en=$(sed -n 1p ${LOG_NAME} | awk '{ print $3 }')
    [ "$en" == "IN_MOVED_FROM" ]
}

@test "hook_in_moved_to" {
    TAB_NAME=etc/incron.d/hook_in_moved_to
    LOG_NAME=log/IN_MOVED_TO.log

    touch tmp/moved_to_filename

    mv tmp/moved_to_filename tmp/watch_IN_MOVED_TO/moved_to_filename

    wait_for_file ${LOG_NAME} 10

    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 1
    [ $? -eq 0 ]

    fn=$(sed -n 1p ${LOG_NAME} | awk '{ print $2 }')
    [ "$fn" == "moved_to_filename" ]

    en=$(sed -n 1p ${LOG_NAME} | awk '{ print $3 }')
    [ "$en" == "IN_MOVED_TO" ]
}

@test "hook_in_open" {
    TAB_NAME=etc/incron.d/hook_in_open
    LOG_NAME=log/IN_OPEN.log

    cat tmp/watch_IN_OPEN > /dev/null

    wait_for_file ${LOG_NAME} 10

    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 1
    [ $? -eq 0 ]

    compare_files ${TAB_NAME} ${LOG_NAME} 2
    [ $? -eq 0 ]
}
