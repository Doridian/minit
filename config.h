#pragma once

#define SERVICES_TEXT "/minit/services"

#include <sys/types.h>

int services_count;

typedef struct __attribute__((__packed__)) procinfo {
	int uid;
	int gid;
	int stop_signal;
	pid_t pid;
	union {
		char *cwd;
		int cwd_rel;
	};
	union {
		char *command;
		int command_rel;
	};
} procinfo;

procinfo *subproc_info;

