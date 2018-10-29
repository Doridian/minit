#define _GNU_SOURCE

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
#define writecheck(fd, data, len) { if (write(fd, data, len) < len) { fprintf(stderr, "Cannot write stdout\n"); return 1; } }

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

#define sigcmp(sig) (strcmp(str, # sig) == 0) { return SIG ## sig; }

int getsignum(const char *str) {
	if (strlen(str) > 3 && str[0] == 'S' && str[1] == 'I' && str[2] == 'G') {
		str += 3;
	}

	if sigcmp(HUP)
	else if sigcmp(INT)
	else if sigcmp(TERM)
	else if sigcmp(KILL)
	else if sigcmp(USR1)
	else if sigcmp(USR2)

	return -1;
}

char *buffer;
size_t buffer_size;
size_t buffer_pos = 0;

int addstring(const char *str) {
	int tmplen = strlen(str) + 1;

	char *bufptr = memmem(buffer, buffer_size, str, tmplen);
	if (bufptr) {
		return (int)(bufptr - buffer);
	}

	int ret = buffer_pos;
	memcpy(buffer + buffer_pos, str, tmplen);
	buffer_pos += tmplen;
	return ret;
}

int main() {
	FILE *src = fopen(SERVICES_FILE, "r");
	if (!src) {
		fprintf(stderr, "Cannot open " SERVICES_FILE "\n");
		return 1;
	}

	char uid_str[1024];
	char gid_str[1024];
	char stop_signal_str[1024];
	char cwd[4096];
	char cmd[65536];

	int uid, gid, stop_signal;

	struct passwd *upasswd;
	struct group *gpasswd;

	while (1) {
		ifscan(src) {
			services_count++;
			buffer_size += strlen(cmd) + 1 + strlen(cwd) + 1;
		} else if (feof(src)) {
			break;
		}
	}

	if (services_count <= 0) {
		fprintf(stderr, "No services defined\n");
		return 1;
	}

	buffer = malloc(buffer_size);
	subproc_info = malloc(sizeof(procinfo) * services_count);

	services_count = 0;
	rewind(src);

	while (1) {
		ifscan(src) {
			if (isnumber(uid_str, 0)) {
				uid = atoi(uid_str);
			} else {
				upasswd = getpwnam(uid_str);
				if (!upasswd) {
					fprintf(stderr, "Don't know user %s, aborting!\n", uid_str);
					return 1;
				} else {
					uid = upasswd->pw_uid;
				}
			}
			if (isnumber(gid_str, 0)) {
				gid = atoi(gid_str);
			} else {
				gpasswd = getgrnam(gid_str);
				if (!gpasswd) {
					fprintf(stderr, "Don't know group %s, aborting!\n", gid_str);
					return 1;
				} else {
					gid = gpasswd->gr_gid;
				}
			}
			if (isnumber(stop_signal_str, 0)) {
				stop_signal = atoi(stop_signal_str);
			} else {
				stop_signal = getsignum(stop_signal_str);
				if (stop_signal < 0) {
					fprintf(stderr, "Don't signal %s, aborting!\n", stop_signal_str);
					return 1;
				}
			}

			subproc_info[services_count].cwd_rel = addstring(cwd);
			subproc_info[services_count].command_rel = addstring(cmd);

			subproc_info[services_count].stop_signal = stop_signal;
			subproc_info[services_count].uid = uid;
			subproc_info[services_count].gid = gid;
			services_count++;
		} else if (feof(src)) {
			break;
		}
	}

	writecheck(1, &buffer_pos, sizeof(buffer_pos));
	writecheck(1, &services_count, sizeof(services_count));
	writecheck(1, buffer, buffer_pos);
	writecheck(1, subproc_info, sizeof(procinfo) * services_count);

	fclose(src);

	free(subproc_info);
	free(buffer);

	return 0;
}

