#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// I did all the runtime checks
// on maximum name and cmdline arguments'
// lengths in ps_lsof, so I feel lazy now :)
#define BUF_SIZE 128

void print_cmdline(int cmdlinefd)
{
	off_t off = lseek(cmdlinefd, 0, SEEK_SET);
	if (off == -1) {
		perror("lseek");
		exit(1);
	}

	char arg_buf[BUF_SIZE] = {0};
	ssize_t file_size = 0;
	for (;;) {
		ssize_t readr = read(cmdlinefd, arg_buf + file_size, BUF_SIZE-file_size);
		if (readr > 0)
			file_size = readr;
		else if (readr == 0)
			break;
		else {
			perror("read");
			exit(1);
		}
	}

	int i = 0;
	for (i = 0; i < file_size; i++) {
		if (arg_buf[i] == '\0')
			arg_buf[i] = ' ';
	}
	arg_buf[file_size] = 0;

	printf("%s\n", arg_buf);
}

int main(int argc, char* argv[])
{
	char cmdline_file_name[BUF_SIZE] = {0};
	pid_t mypid = getpid();
	sprintf(cmdline_file_name, "/proc/%d/cmdline", mypid);

	int cmdlinefd = open(cmdline_file_name, O_RDONLY);
	if (cmdlinefd == -1) {
		perror("open");
		return 1;
	}

	printf("Initial cmdline contents:\n");
	print_cmdline(cmdlinefd);

	int r = prctl(PR_SET_MM, PR_SET_MM_ARG_START, argv[0]+strlen(argv[0]), 0, 0);
	if (r == -1) {
		perror("prctl");
		return 1;
	}

	printf("Prctl succeded, new cmdline contents:\n");
	print_cmdline(cmdlinefd);

	close(cmdlinefd);

	return 0;
}
