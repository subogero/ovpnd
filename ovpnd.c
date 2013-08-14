#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>

int logfd;

static int service(char *cmd);

int main(int argc, char *argv[]) {
	/* Fork the real daemon */
	pid_t pid = fork();
	if (pid < 0)
		return 1;
	if (pid > 0) {
		int status;
		pid_t pid = wait(&status);
		printf("Daemon %d exited with %d\n", pid, WEXITSTATUS(status));
		return 0;
	}
	/* umask and session ID */
	umask(0);
	pid_t sid = setsid();
	if (sid < 0)
		return 2;
	/* Run in erm... /var/run */
	if (chdir("/var/run/") < 0)
		return 3;
	/* Create log file as stdout and stderr */
	close(0);
	logfd = open("ovpnd.stat", O_CREAT|O_WRONLY, 0644);
	if (logfd < 0)
		return 4;
	close(1);
	logfd = dup(logfd);
	if (write(logfd, "ovpnd started\n", 14) < 14)
		return 5;
	close(2);
	dup(logfd);
	/* Create and open FIFO for command input as stdin */
	unlink("ovpnd.cmd");
	write(logfd, "Deleted original ovpnd.cmd FIFO\n", 32);
	if (mknod("ovpnd.cmd", S_IFIFO | 0666, 0) < 0)
		return 6;
	write(logfd, "Created new ovpnd.cmd FIFO\n", 27);
	close(0);
	int cmdfd = open("ovpnd.cmd", O_RDONLY|O_NONBLOCK);
	if (cmdfd < 0)
		return 7;
	write(logfd, "Opened ovpnd.cmd FIFO\n", 22);
	/* Main loop */
	char cmd;
	while (1) {
		/*
		 * If proc writing to FIFO closes, read()s return with 0.
		 * Close FIFO, open() blocks until next proc writes in it.
		 */
		if (read(cmdfd, &cmd, 1) == 0) {
			continue;
		}
		switch (cmd) {
		case 'u':
			if (service("start") != 0)
				return 8;
			break;
		case 'd':
			if (service("stop") != 0)
				return 8;
			break;
		case 'r':
			if (service("restart") != 0)
				return 8;
			break;
		case 'q':
			write(logfd, "Bye\n", 4);
			if (service("stop") != 0)
				return 8;
			return 0;
		default:
			break;
		}
	}
	return 0;
}

static int service(char *cmd)
{
	pid_t pid = fork();
	if (pid > 0) {
		wait(NULL);
		return 0;
	} else if (pid == 0) {
		char *argv[3];
		argv[0] = "/etc/init.d/openvpn";
		argv[1] = cmd;
		argv[2] = NULL;
		close(0);
		execve(argv[0], argv, NULL);
		return 1;
	} else {
		return 2;
	}
}
