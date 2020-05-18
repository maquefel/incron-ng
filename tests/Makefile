# SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
# SPDX-License-Identifier: CC0-1.0

# Requirements:
#  /bin/bash as SHELL
export SHELL = /bin/bash

CC=gcc
CFLAGS?=-Wall -std=gnu11 -D_GNU_SOURCE -fPIC -O0 -DDEBUG -ggdb -Wno-unused-variable -fno-omit-frame-pointer
LDFLAGS+= -lcheck -lrt -lpthread -lm -lsubunit

INCLUDE += -I../src/

define _newline


endef

define multiline_echo
echo -e '$(call subst,${_newline},\n,${1})'
endef

all: tests

.PHONY: debug
debug: CFLAGS+= -O0 -DDEBUG -ggdb -g3 -Wno-unused-variable
debug: all

.PHONY: gcov
gcov: CFLAGS+= -fprofile-arcs -ftest-coverage
gcov: debug

.PHONY: asan
asan: CFLAGS+= -fsanitize=address -fsanitize=leak
asan: gcov

ETC_DIR ?= ${CURDIR}/etc
SYSTEM_TABLE_DIR = ${ETC_DIR}/incron.d
ALLOWED_USERS = ${ETC_DIR}/incron.allow
DENIED_USERS = ${ETC_DIR}/incron.deny

VAR_SPOLL_DIR ?= ${CURDIR}/var/spool
USER_TABLE_DIR = ${VAR_SPOLL_DIR}/incron

LOCKFILE_DIR ?= ${CURDIR}/var/run
LOCKFILE_NAME = incrond

TEST_USER = user_hook_test

.PHONY: create-files

create-files: ${ETC_DIR}/incron.conf \
	${ALLOWED_USERS} \
	${DENIED_USERS} \
	${LOCKFILE_DIR} \
	${LOCKFILE_NAME} \
	${SYSTEM_TABLE_DIR} \
	${USER_TABLE_DIR}/${TEST_USER} \
	log \
	tmp

${ETC_DIR}:
	mkdir -p $@

${SYSTEM_TABLE_DIR}: ${ETC_DIR}
	mkdir -p $@

${VAR_SPOLL_DIR}:
	mkdir -p $@

${USER_TABLE_DIR}: ${VAR_SPOLL_DIR}
	mkdir -p $@

${LOCKFILE_DIR}:
	mkdir -p $@

${SYSROOT}/.mount-stamp:	| ${SYSROOT}
	touch $@

log:
	mkdir -p $@

tmp:
	mkdir -p $@

define incron_conf
system_table_dir	=	$(SYSTEM_TABLE_DIR)
user_table_dir		=	$(USER_TABLE_DIR)
allowed_users		=	$(ALLOWED_USERS)
denied_users		=	$(DENIED_USERS)
lockfile_dir		=	$(LOCKFILE_DIR)
lockfile_name		=	$(LOCKFILE_NAME)
editor =
endef

${ETC_DIR}/incron.conf:	| ${ETC_DIR}
	@$(call multiline_echo,${incron_conf}) > ${ETC_DIR}/incron.conf

${ALLOWED_USERS}:	${ETC_DIR}
	@echo "incron" > $@
	@echo ${TEST_USER} >> $@

$(DENIED_USERS):	${ETC_DIR}
	@echo "incron_deny" > $@

$(LOCKFILE_DIR)/${LOCKFILE_NAME}:	$(LOCKFILE_DIR)

${LOCKFILE_NAME}:	$(LOCKFILE_DIR)/${LOCKFILE_NAME}

INCRON_FLAGS := \
IN_ACCESS \
IN_ATTRIB \
IN_CLOSE_WRITE \
IN_CLOSE_NOWRITE \
IN_CREATE \
IN_DELETE \
IN_DELETE_SELF \
IN_MODIFY \
IN_MOVE_SELF \
IN_MOVED_FROM \
IN_MOVED_TO \
IN_OPEN

define lc
$(shell echo $(1) | tr A-Z a-z)
endef

define init_flag
ifeq (,$(filter IN_CREATE IN_DELETE IN_DELETE_SELF IN_MOVED_FROM IN_MOVED_TO,$(1)))
${SYSTEM_TABLE_DIR}/hook_$(call lc,$(1)):
	@echo '${CURDIR}/tmp/watch_$(1) $(1) echo $$$$@ $$$$# $$$$% $$$$& $$$$$$$$ > ${CURDIR}/log/$(1).log' > $$@
	@touch ${CURDIR}/tmp/watch_$(1)
else
${SYSTEM_TABLE_DIR}/hook_$(call lc,$(1)):
	@echo '${CURDIR}/tmp/watch_$(1)/ $(1) echo $$$$@ $$$$# $$$$% $$$$& $$$$$$$$ >> ${CURDIR}/log/$(1).log' > $$@
	@mkdir ${CURDIR}/tmp/watch_$(1)
endif
endef

$(foreach f,${INCRON_FLAGS},$(eval $(call init_flag,$(f))))

.PHONY: create-hooks

INCRON_FLAGS_LC := $(foreach f,$(INCRON_FLAGS),$(call lc,$(f)))

create-hooks: $(patsubst %,${SYSTEM_TABLE_DIR}/%,$(addprefix hook_,${INCRON_FLAGS_LC}))
	@touch ${CURDIR}/tmp/watch_user_exec

clean::
	-rm -rf tmp
	-rm -rf log
	-rm -rf etc
	-rm -rf log
	-rm -rf var

create-recursive: ${ETC_DIR}/incron.conf | ${USER_TABLE_DIR} ${SYSTEM_TABLE_DIR} ${LOCKFILE_DIR} ${LOCKFILE_NAME} ${SYSTEM_TABLE_DIR} ${ALLOWED_USERS} ${DENIED_USERS} log
	@echo '${CURDIR}/tmp/watch_RECURSIVE IN_ALL_EVENTS echo $$@ $$# $$% $$& >> ${CURDIR}/log/RECURSIVE.log' > etc/incron.d/hook_in_recursive
	@mkdir -p ${CURDIR}/tmp/watch_RECURSIVE

${USER_TABLE_DIR}/${TEST_USER}:	| ${USER_TABLE_DIR}
	@echo '${CURDIR}/tmp/watch_user_exec IN_ACCESS echo $$(whoami) $$(pwd) > /tmp/watch_user_exec.log' > $@

TESTS=parse-tabs-test parse-config-test parse-users-test

$(TESTS) :
	$(CC) $(CFLAGS) -o $@ $(@).c $(LDFLAGS)

TESTS_BATS=incron_test_hooks.bats

$(TESTS_BATS) : create-files create-hooks

.PHONY: tests

tests: $(TESTS) $(TESTS_BATS)
	for t in $(TESTS) ; do \
		CK_FORK=no $$(realpath $$t) ; \
	done
	for t in $(TESTS_BATS) ; do \
		bats $$(realpath $$t) ; \
	done
	bats incron_test_user_exec.bats

clean::
	rm -rf $(TESTS) $(addsuffix .o,$(TESTS))
	-rm *.gcda *.gcno
