/* (c) SZABO Gergely <szg@subogero.com>, license WTFPL 2.0 */
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

int logfd;

static int service(char *cmd);
static void writedec(int fd, int num);

int main(int argc, char *argv[]) {
	/* Client when called with u d r q options */
	if (argc > 1) {
		int cmdfd = open("/var/run/ovpnd.cmd", O_WRONLY|O_NONBLOCK);
		if (cmdfd < 0) {
			write(2, "Can't open /var/run/ovpnd.cmd\n", 31);
			return 10;
		}
		if (write(cmdfd, argv[1], 1) == 0) {
			write(2, "Can't write /var/run/ovpnd.cmd\n", 32);
			return 11;
		}
		return 0;
	}
	/* Fork the real daemon */
	pid_t pid = fork();
	if (pid < 0)
		return 1;
	if (pid > 0) {
		write(1, "ovpnd daemon started, PID ", 26);
		writedec(1, pid);
		write(1, "\n", 1);
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
	logfd = creat("ovpnd.stat", 0644);
	if (logfd < 0)
		return 4;
	close(1);
	logfd = dup(logfd);
	writedec(logfd, sid);
	if (write(logfd, " ovpnd started\n", 15) < 15)
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
	int cmdfd = open("ovpnd.cmd", O_RDONLY);
	if (cmdfd < 0)
		return 7;
	/* Main loop */
	char cmd;
	while (1) {
		/*
		 * If writing proc has closed the FIFO, we close it too.
		 * Next open() blocks until a new process opens write end.
		 */
		if (read(cmdfd, &cmd, 1) == 0) {
			close(cmdfd);
			int cmdfd = open("ovpnd.cmd", O_RDONLY);
			if (cmdfd < 0)
				return 7;
			write(logfd, "Opened ovpnd.cmd FIFO\n", 22);
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

/* Start or stop the actual openvpn daemon */
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

/* Write number in decimal format to file descriptor, printf() is BLOATED!!! */
static void writedec(int fd, int num)
{
	/* Special cases: zero and negative numbers (print neg.sign) */
	if (num == 0) {
		write(fd, "0", 1);
		return;
	}
	if (num < 0) {
		write(fd, "-", 1);
		num *= -1;
	}
	/*
	 * If num >= 10, print More Significant DigitS first by recursive call
	 * then we print Least Significatn Digit ourselves.
	 */
	int msds = num / 10;
	int lsd = num % 10;
	if (msds)
		writedec(fd, msds);
	char digit = '0' + lsd;
	write(fd, &digit, 1);
}
