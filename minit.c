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
pid_t pid;

int numServices;
pid_t subproc[MAX_NUMSERVICES];

struct procinfo {
	int uid;
	int gid;
	int stop_signal;
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
	int i, subproc_sig;
	if (shouldRun == 1) {
		shouldRun = 0;
	}

	if (signum == SIGPWR) {
		signum = SIGINT;
	}

	for (i = 0; i < numServices; i++) {
		if (subproc[i]) {
			subproc_sig = subproc_info[i].stop_signal;
			if (!subproc_sig) {
				subproc_sig = signum;
			}
			kill(subproc[i], subproc_sig);
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
				if (subproc[i] == chld) {
					runproc(i, 1);
					break;
				}
			}
		}
		// We do not actually care
	}
}

void spawnNormalInit(int signum) {
	
}

void load() {
	if (system("/minit/load")) {
		exit(1);
	}

	shouldRun = 2;
	signal(SIGCHLD, sigchldHandler);

	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGPWR, signalHandler);
	signal(SIGUSR1, spawnNormalInit);

	system(ONBOOT_TMP);

	numServices = 0;

	FILE *fh = fopen(SERVICES_TMP, "r");
	int uid, gid, stop_signal;
	char cwd[4096];
	char cmd[65536];
	int srv;
	while (1) {
		if (fscanf(fh, "%d %d %d %4095s %65535[^\n]\n", &stop_signal, &uid, &gid, cwd, cmd) == 5) {
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
			subproc_info[srv].stop_signal = stop_signal;
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
	pid = getpid();

	load();
	closeall();
	run();
	while (shouldRun == 1) {
		sleep(1);
	}

	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGPWR, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);

	kill(-pid, SIGTERM);
	shutdown();
	sleep(1);
	
	if (shouldRun == 2) {
		execl("/sbin/init", "init", NULL);
	}
	
	kill(-pid, SIGKILL);

	return 0;
}

