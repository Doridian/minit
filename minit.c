#define MAX_NUMSERVICES 16
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
struct procinfo {
	int uid;
	int gid;
	int stop_signal;
	pid_t pid;
	char *cwd;
	char *command;
};

struct procinfo subproc_info[MAX_NUMSERVICES];

void runproc(const int index, const int slp) {
	struct procinfo info = subproc_info[index];
	pid_t fpid = vfork();
	if (fpid == 0) {
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
		if (slp > 0) {
			sleep(slp);
		}
		execl("/bin/sh", "sh", "-c", info.command, NULL);
		_exit(1);
	} else if (fpid < 0) {
		exit(1);
	}
	subproc_info[index].pid = fpid;
}

void signalHandler(int signum) {
	int i, subproc_sig;
	if (shouldRun == 1) {
		shouldRun = 0;
	}

	if (signum == SIGPWR) {
		signum = SIGINT;
	}

	for (i = 0; i < numServices; i++) {
		if (subproc_info[i].pid) {
			subproc_sig = subproc_info[i].stop_signal;
			if (!subproc_sig) {
				subproc_sig = signum;
			}
			kill(subproc_info[i].pid, subproc_sig);
		}
	}
}

void sigchldHandler(int signum) {
	int i;
	pid_t chld;
	while ((chld = waitpid(-1, &childExitStatus, WNOHANG)) > 0) {
		if (shouldRun == 1) {
			// Try to find it
			for (i = 0; i < numServices; i++) {
				if (subproc_info[i].pid == chld) {
					runproc(i, 1);
					break;
				}
			}
		}
		// We do not actually care
	}
}

void spawnNormalInit(int signum) {
	shouldRun = 2;
}

void load() {
	shouldRun = 1;
	signal(SIGCHLD, sigchldHandler);

	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGPWR, signalHandler);
	signal(SIGUSR1, spawnNormalInit);

	system("/minit/onboot");

	numServices = -1;

	FILE *fh = fopen("/minit/services", "r");
	int uid, gid, stop_signal;
	char cwd[4096];
	char cmd[65536];
	int total_size = 0;
	while (1) {
		if (fscanf(fh, "%d %d %d %4095s %65535[^\n]\n", &stop_signal, &uid, &gid, cwd, cmd) == 5) {
			numServices++;
			if (numServices >= MAX_NUMSERVICES) {
				exit(1);
			}

			total_size += strlen(cmd) + 1 + strlen(cwd) + 1;
		} else if (feof(fh)) {
			break;
		}
	}

	rewind(fh);

	numServices = -1;

	char* mainpage = malloc(total_size);
	int cur_offset = 0;

	while (1) {
		if (fscanf(fh, "%d %d %d %4095s %65535[^\n]\n", &stop_signal, &uid, &gid, cwd, cmd) == 5) {
			numServices++;

			char *realcmd = mainpage + cur_offset;
			cur_offset += strlen(cmd) + 1;
			strcpy(realcmd, cmd);
			char *realcwd = mainpage + cur_offset;
			cur_offset += strlen(cwd) + 1;
			strcpy(realcwd, cwd);

			subproc_info[numServices].uid = uid;
			subproc_info[numServices].gid = gid;
			subproc_info[numServices].stop_signal = stop_signal;
			subproc_info[numServices].command = realcmd;
			subproc_info[numServices].cwd = realcwd;
		} else if (feof(fh)) {
			break;
		}
	}

	numServices++;

	fclose(fh);
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
		if (subproc_info[srv].pid) {
			waitpid(subproc_info[srv].pid, &childExitStatus, 0);
		}
	}
}

int main() {
	load();
	run();
	while (shouldRun == 1) {
		sleep(1);
	}

	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGPWR, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);

	kill(-getpid(), SIGTERM);
	shutdown();
	sleep(1);
	
	if (shouldRun == 2) {
		execl("/sbin/init", "/sbin/init", NULL);
	}
	
	kill(-getpid(), SIGKILL);

	return 0;
}

