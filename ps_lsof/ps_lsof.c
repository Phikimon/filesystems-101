#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
	int procfd = open("/proc", O_DIRECTORY);
	if (procfd == -1) {
		perror("open");
		return 1;
	}

	DIR* procdir = fdopendir(procfd);
	if (!procdir) {
		perror("fdopendir");
		return 1;
	}

	errno = 0;
	long max_name_len = fpathconf(procfd, _PC_NAME_MAX);
	if (max_name_len == -1) {
		if (errno == 0)
			max_name_len = 255; // Limit not defined, take a guess
		else {
			perror("fpathconf");
			return 1;
		}
	}

	char* file_name_buf = calloc(max_name_len + sizeof((char)'\0'), sizeof(*file_name_buf));
	if (!file_name_buf) {
		perror("calloc");
		return 1;
	}


	long max_arg_len = sysconf(_SC_ARG_MAX);
	char* arg_buf = calloc(max_arg_len + sizeof((char)'\0'), sizeof(arg_buf[0]));
	if (!arg_buf) {
		perror("calloc");
		return 1;
	}

	printf("PID  \tCOMMAND(50 chars limit)\n");
	for (;;) {
		// OPEN PROCESS'S DIRECTORY IN /PROC
		errno = 0;
		struct dirent* procdent = readdir(procdir);
		if (errno) {
			perror("readdir");
			return 1;
		}
		// last element or some standard-compliant glitch with 0-length d_name
		if ((!procdent) || (!procdent->d_name[0]))  break;
		// '.' or '..'
		if ((*procdent->d_name <= '0') || (*procdent->d_name > '9')) continue;

		unsigned long tgid = strtoul(procdent->d_name, NULL, 10);
		sprintf(file_name_buf, "/proc/%lu", tgid);
		int tgidfd = open(file_name_buf, O_DIRECTORY);
		if (tgidfd == -1) {
			if (errno == EACCES)
				continue;
			perror("open");
			return 1;
		}

		// READ CMDLINE FILE
		// openat() is used to make sure there is no race
		// and this is not another process with same PID
		int cmdlinefd = openat(tgidfd, "cmdline", O_RDONLY);
		if (cmdlinefd == -1) {
			if (errno == EACCES)
				goto close_tgidfd;
			perror("openat");
			return 1;
		}

		ssize_t file_size = 0;
		for (;;) {
			ssize_t readr = read(cmdlinefd, arg_buf + file_size, max_arg_len);
			if (readr > 0)
				file_size = readr;
			else if (readr == 0)
				break;
			else {
				perror("read");
				return 1;
			}
		}
		(void)close(cmdlinefd);

		// PRINT CMDLINE
		int i = 0;
		for (i = 0; i < file_size; i++) {
			if (arg_buf[i] == '\0')
				arg_buf[i] = ' ';
		}
		arg_buf[file_size] = 0;

		printf("%-5lu\t'%.50s'\n", tgid, arg_buf);

		// PRINT OPEN FDS

		// To open fd/ dir without absolute paths
		// we are forced to open this directory
		// with openat() first.

		sprintf(file_name_buf, "fd/");
		// I know, naming is just great
		int fdsdirfd = openat(tgidfd, file_name_buf, O_DIRECTORY);
		if (fdsdirfd == -1) {
			if (errno == EACCES)
				goto close_tgidfd;
			perror("openat");
			return 1;
		}

		DIR* fdsdir = fdopendir(fdsdirfd);
		if (!fdsdir) {
			if (errno == EACCES)
				goto close_fdsdirfd;
			perror("fdopendir");
			return 1;
		}

		for (;;) {
			errno = 0;
			struct dirent* fddent = readdir(fdsdir);
			if (errno) {
				perror("readdir");
				return 1;
			}
			// last element or some standard-compliant glitch with 0-length d_name
			if ((!fddent) || (!fddent->d_name[0]))  break;
			// '.' or '..'
			if ((*fddent->d_name <= '0') || (*fddent->d_name > '9')) continue;
			unsigned long fdnum = strtoul(fddent->d_name, NULL, 10);

			char fd_relative_path[64] = {0};
			sprintf(fd_relative_path, "fd/%lu", fdnum);
			ssize_t link_len = readlinkat(tgidfd, fd_relative_path, file_name_buf, max_name_len);
			if (link_len == -1) {
				perror("readlinkat");
				return 1;
			}
			file_name_buf[link_len] = '\0';

			printf("\tfd %-3lu\t-> %s\n", fdnum, file_name_buf);
		}

		(void)closedir(fdsdir);
close_fdsdirfd:
		(void)close(fdsdirfd);
close_tgidfd:
		(void)close(tgidfd);
	}

	free(arg_buf);
	free(file_name_buf);
	(void)closedir(procdir);
	(void)close(procfd);
	return 0;
}
