#pragma once

#define MAX_NUMSERVICES 16
#define TOTAL_SIZE_T int

#define SERVICES_TEXT "/minit/services"

#include <sys/types.h>

int numServices;

struct procinfo {
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
};

struct procinfo subproc_info[MAX_NUMSERVICES];

