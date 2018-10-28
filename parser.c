#include "config.h"

#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>

#define ifscan(FH) if (fscanf(FH, "%1023s %1023s %1023s %4095s %65535[^\n]\n", stop_signal_str, uid_str, gid_str, cwd, cmd) == 5)
#define bufwrite(data) { int tmplen = strlen(data) + 1; memcpy(buffer + bufferpos, data, tmplen); bufferpos += tmplen; }

int isnumber(const char *str, int allowspace) {
	do {
		if (!isdigit(*str)) {
			if (allowspace && isspace(*str)) {
				return 1;
			}
			return 0;
		}
	} while(*++str);

	return 1;
}

int getsignum(const char *str, int mode) {
	int pipefd[2];
	if (pipe(pipefd)) {
		return -1;
	}

	pid_t pid = vfork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	} else if (pid) {
		close(pipefd[1]);
		int status;
		waitpid(pid, &status, 0);
		char buf[4096];
		if (read(pipefd[0], buf, 4095) < 1) {
			return -2;
		}
		if (!isnumber(buf, 1)) {
			return -3;
		}
		int ret = atoi(buf);
		close(pipefd[0]);
		return ret;
	}

	close(pipefd[0]);
	close(1);
	dup2(pipefd[1], 1);
	if (mode == 0) {
		execl("/bin/kill", "kill", "-l", NULL);
	} else if (mode == 1) {
		char *argl = malloc(strlen(str) + 1 + 2);
		sprintf(argl, "-l%s", str);
		execl("/bin/kill", "kill", argl, NULL);
	}
	_exit(1);
}

int main() {
	FILE *src = fopen(SERVICES_TEXT, "r");
	if (!src) {
		return 1;
	}
	FILE *dst = stdout;

	char uid_str[1024];
	char gid_str[1024];
	char stop_signal_str[1024];
	char cwd[4096];
	char cmd[65536];

	int uid, gid, stop_signal;

	struct passwd *upasswd;
	struct group *gpasswd;

	TOTAL_SIZE_T total_size = 0;

	while (1) {
		ifscan(src) {
			numServices++;
			if (numServices >= MAX_NUMSERVICES) {
				printf("Out of service space, max %d\n", MAX_NUMSERVICES);
				exit(1);
			}

			total_size += strlen(cmd) + 1 + strlen(cwd) + 1;
		} else if (feof(src)) {
			break;
		}
	}

	char *buffer = malloc(total_size);
	int bufferpos = 0;

	numServices = 0;
	rewind(src);

	while (1) {
		ifscan(src) {
			if (isnumber(uid_str, 0)) {
				uid = atoi(uid_str);
			} else {
				upasswd = getpwnam(uid_str);
				uid = upasswd->pw_uid;
			}
			if (isnumber(gid_str, 0)) {
				gid = atoi(gid_str);
			} else {
				gpasswd = getgrnam(gid_str);
				gid = gpasswd->gr_gid;
			}
			if (isnumber(stop_signal_str, 0)) {
				stop_signal = atoi(stop_signal_str);
			} else {
				stop_signal = getsignum(stop_signal_str, 0);
				if (stop_signal < 0) {
					stop_signal = getsignum(stop_signal_str, 1);
					if (stop_signal < 0) {
						printf("Don't know how to convert %s to signal, using default\n", stop_signal_str);
						stop_signal = 0;
					}
				}
			}

			subproc_info[numServices].cwd_rel = bufferpos;
			bufwrite(cwd);
			subproc_info[numServices].command_rel = bufferpos;
			bufwrite(cmd);

			subproc_info[numServices].stop_signal = stop_signal;
			subproc_info[numServices].uid = uid;
			subproc_info[numServices].gid = gid;

			numServices++;
		} else if (feof(src)) {
			break;
		}
	}

	fwrite(&total_size, sizeof(total_size), 1, dst);
	fwrite(buffer, total_size, 1, dst);
	fwrite(&numServices, sizeof(numServices), 1, dst);
	fwrite(&subproc_info, sizeof(struct procinfo), numServices, dst);

	fclose(src);
	fclose(dst);

	return 0;
}

