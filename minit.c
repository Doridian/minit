#include "config.h"

#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#define readcheck(fh, dst, size) { if(read(fh, dst, size) < size) { exit(9); } }

static int childExitStatus;
static int shouldRun;
static char* mainpage = NULL;

static inline void runproc(const int index, const int slp) {
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
		execl(SH_BINARY, SH_BINARY, "-c", info.command, NULL);
		_exit(1);
	} else if (fpid < 0) {
		shouldRun = 0;
		subproc_info[index].pid = 0;
		return;
	}
	subproc_info[index].pid = fpid;
}

void signalHandler(int signum) {
	int i, subproc_sig;
	if (shouldRun == 1) {
		shouldRun = 0;
	}

	if (signum == SIGPWR || signum == SIGUSR1 || signum == SIGUSR2) {
		signum = SIGTERM;
	}

	for (i = 0; i < services_count; i++) {
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
			for (i = 0; i < services_count; i++) {
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

static inline void load() {
	shouldRun = 1;
	signal(SIGCHLD, sigchldHandler);

	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGPWR, signalHandler);
	signal(SIGUSR2, signalHandler);

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
		execl(PARSER_BINARY, PARSER_BINARY, NULL);
		_exit(1);
	}

	int status;
	close(pipefd[1]);
	waitpid(subpid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
		exit(10 + WEXITSTATUS(status));
	}

	int fh = pipefd[0];

	size_t strings_size;
	readcheck(fh, &strings_size, sizeof(strings_size));
	readcheck(fh, &services_count, sizeof(services_count));
	int subproc_size = sizeof(procinfo) * services_count;

	mainpage = malloc(strings_size + subproc_size);
	readcheck(fh, mainpage, strings_size + subproc_size);

	subproc_info = (void*)mainpage + strings_size;

	for (int i = 0; i < services_count; i++) {
		subproc_info[i].pid = 0;
		subproc_info[i].command = subproc_info[i].command_rel + mainpage;
		subproc_info[i].cwd = subproc_info[i].cwd_rel + mainpage;
	}

	close(fh);

	printf("Loaded %d services\n", services_count);

	if (system(ONBOOT_FILE)) {
		printf(ONBOOT_FILE " failed!\n");
		exit(6);
	}
}

static inline void run() {
	int srv;
	for (srv = 0; srv < services_count; srv++) {
		runproc(srv, 0);
	}
}

static inline void shutdown() {
	int srv;
	for (srv = 0; srv < services_count; srv++) {
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
	signal(SIGHUP, SIG_IGN);

	signal(SIGTERM, SIG_IGN);
	signal(SIGPWR, SIG_IGN);

	signal(SIGCHLD, SIG_IGN);

	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);

	kill(-getpid(), SIGTERM);
	shutdown();
	sleep(1);

	if (mainpage) {
		free(mainpage);
		mainpage = NULL;
	}
	
	if (shouldRun == 2) {
		execl(REAL_INIT_BINARY, REAL_INIT_BINARY, NULL);
	}
	
	kill(-getpid(), SIGKILL);

	return 0;
}

