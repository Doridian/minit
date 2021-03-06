#pragma once

#define SERVICES_FILE "/etc/minit/services"
#define ONBOOT_FILE "/etc/minit/onboot"

#define MAIN_BINARY "/sbin/minit"
#define PARSER_BINARY "/sbin/minit_parser"

#define REAL_INIT_BINARY "/sbin/init"
#define SH_BINARY "/bin/sh"

#include <sys/types.h>

int services_count;

typedef struct __attribute__((__packed__)) procinfo {
	int uid;
	int gid;
	int stop_signal;
	pid_t pid;
	union {
		char *cwd;
		size_t cwd_rel;
	};
	union {
		char *command;
		size_t command_rel;
	};
} procinfo;

procinfo *subproc_info;

