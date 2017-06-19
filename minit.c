#define MAX_NUMSERVICES 16
#define ONBOOT_TMP "/tmp/minit_onboot"
#define SERVICES_TMP "/tmp/minit_services"

#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/signal.h>
#include <string.h>

int childExitStatus;
int shouldRun;

int numServices;
pid_t subproc[MAX_NUMSERVICES];

struct procinfo {
	int uid;
	int gid;
	char *cwd;
	char *command;
};

struct procinfo subproc_info[MAX_NUMSERVICES];

void closeall() {
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);
}

void runproc(const int index, const int slp) {
	struct procinfo info = subproc_info[index];
	subproc[index] = vfork();
	if (subproc[index] == 0) {
		chdir(info.cwd);
		if (info.gid) {
			if (setregid(info.gid, info.gid)) {
				exit(1);
			}
		}
		if (info.uid) {
			if (setreuid(info.uid, info.uid)) {
				exit(1);
			}
		}
		closeall();
		if (slp > 0) {
			sleep(slp);
		}
		execl("/bin/sh", "sh", "-c", info.command, NULL);
		_exit(1);
	} else if (subproc[index] < 0) {
		exit(1);
	}
}

void signalHandler(int signum) {
	int i;
	shouldRun = 0;

	if (signum == SIGPWR) {
		signum = SIGINT;
	}

	for (i = 0; i < numServices; i++) {
		if (subproc[i]) {
			kill(subproc[i], signum);
		}
	}
}

void sigchldHandler(int signum) {
	int i;
	pid_t chld;
	while ((chld = waitpid(-1, &childExitStatus, WNOHANG)) > 0) {
		if (shouldRun) {
			// Try to find it
			for (i = 0; i < numServices; i++) {
				if (subproc[i] == chld) {
					runproc(i, 1);
					break;
				}
			}
		}
		// We do not actually care
	}
}

void load() {
	if (system("/minit/load")) {
		exit(1);
	}

	shouldRun = 1;
	signal(SIGCHLD, sigchldHandler);

	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGPWR, signalHandler);

	system(ONBOOT_TMP);

	numServices = 0;

	FILE *fh = fopen(SERVICES_TMP, "r");
	int uid, gid;
	char cwd[4096];
	char cmd[65536];
	int srv;
	while (1) {
		if (fscanf(fh, "%d %d %4095s %65535[^\n]\n", &uid, &gid, cwd, cmd) == 4) {
			srv = numServices++;
			if (srv >= MAX_NUMSERVICES) {
				exit(1);
			}
			
			char *realcmd = malloc(strlen(cmd) + 1);
			char *realcwd = malloc(strlen(cwd) + 1);
			strcpy(realcmd, cmd);
			strcpy(realcwd, cwd);
			
			subproc_info[srv].uid = uid;
			subproc_info[srv].gid = gid;
			subproc_info[srv].command = realcmd;
			subproc_info[srv].cwd = realcwd;
		} else if (feof(fh)) {
			break;
		}
	}
	fclose(fh);

	unlink(ONBOOT_TMP);
	unlink(SERVICES_TMP);
}

void run() {
	int srv;
	for (srv = 0; srv < numServices; srv++) {
		runproc(srv, 0);
		sleep(1);
	}
}

void shutdown() {
	int srv;
	for (srv = 0; srv < numServices; srv++) {
		if (subproc[srv]) {
			waitpid(subproc[srv], &childExitStatus, 0);
		}
	}
}

int main() {
	load();
	closeall();
	run();
	while (shouldRun) {
		sleep(1);
	}
	shutdown();
	return 0;
}

