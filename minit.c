#include "config.h"

#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#define readcheck(fh, dst, size) { if(read(fh, dst, size) < size) { exit(7); } }

int childExitStatus;
int shouldRun;

void runproc(const int index, const int slp) {
	struct procinfo info = subproc_info[index];
	pid_t fpid = vfork();
	if (fpid == 0) {
		if (chdir(info.cwd)) {
			exit(1);
		}
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
		exit(5);
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

	int pipefd[2];
	if (pipe(pipefd)) {
		exit(7);
	}

	pid_t subpid = vfork();
	if (subpid < 0) {
		exit(8);
	} else if (subpid == 0) {
		close(pipefd[0]);
		close(1);
		dup2(pipefd[1], 1);
		execl("/minit/parser", "parser", NULL);
		_exit(1);
	}

	int status;
	close(pipefd[1]);
	waitpid(subpid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
		exit(6);
	}

	if (system("/minit/onboot")) {
		printf("onboot failed, ignoring...\n");
	}

	int fh = pipefd[0];

	numServices = -1;

	TOTAL_SIZE_T total_size = 0;
	readcheck(fh, &total_size, sizeof(total_size));
	char* mainpage = malloc(total_size);
	readcheck(fh, mainpage, total_size);

	readcheck(fh, &numServices, sizeof(numServices));
	int subproc_size = sizeof(procinfo) * numServices;
	subproc_info = malloc(subproc_size);
	readcheck(fh, subproc_info, subproc_size);

	for (int i = 0; i < numServices; i++) {
		subproc_info[i].command = subproc_info[i].command_rel + mainpage;
		subproc_info[i].cwd = subproc_info[i].cwd_rel + mainpage;
	}

	close(fh);

	printf("Loaded %d services\n", numServices);
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

