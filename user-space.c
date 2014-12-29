#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DEFAULT_DEVICE		"/dev/ml0"
#define DEFAULT_DURATION	800

#define ML_STOP         0x00
#define ML_UP           0x01
#define ML_DOWN         0x02
#define ML_LED          0x03
#define ML_LEFT         0x04
#define ML_RIGHT        0x08
#define ML_FIRE         0x10

#define ML_FIRE_DELAY	5000

void send_cmd(int fd, int cmd)
{
	int retval = 0;

	retval = write(fd, &cmd, 1);
	if (retval < 0)
		fprintf(stderr, "could not send command to fd=%d\n", fd);
}

static void usage(char *name)
{
	fprintf(stderr,
			"\nusage: %s [-mbslrudfch] [-t <msecs>]\n\n"
			"  -m      missile launcher [/dev/ml0]\n"
			"  -s      stop\n"
			"  -l      turn left\n"
			"  -r      turn right\n"
			"  -u      turn up\n"
			"  -d      turn down\n"
			"  -f      fire\n"
			"  -b      blink leds\n"
			"  -t      specify send_cmdment duration in milli seconds\n"
			"  -c      calibrate the missile launcher axes\n"
			"  -h      display this help\n\n"
			"notes:\n"
			"* it is possible to combine the directions of the two axes, e.g.\n"
			"  '-lu' send_cmds the missile launcher up and left at the same time.\n"
			"" , name);
	exit(1);
}


int main(int argc, char *argv[])
{
	char c;
	int fd;
	int cmd = ML_STOP;
	int duration = DEFAULT_DURATION;
	char *dev = DEFAULT_DEVICE;

	if (argc < 2)
		usage(argv[0]);

	while ((c = getopt(argc, argv, "m:bslrudfcht:")) != -1) {
		switch (c) {
			case 'm': dev = optarg;
					  break;
			case 'l': cmd |= ML_LEFT;
					  break;
			case 'r': cmd |= ML_RIGHT;
					  break;
			case 'u': cmd |= ML_UP;
					  break;
			case 'd': cmd |= ML_DOWN;
					  break;
			case 'f': cmd = ML_FIRE;
					  break;
			case 'b': cmd = ML_LED;
					  break;
			case 's': cmd = ML_STOP;
					  break;
			case 't': duration = atoi(optarg);
					  break;
			case 'c': /* 
					  retval = ioctl(fd, ML_IOCTCALIBRATE, ML_CALIBRATE_FULL);
					  if (retval < 0) 
						  perror("ioctl");
					  */
					  break;
			default: usage(argv[0]);
		}
	}

	printf("Open device %s\n", dev);
	fd = open(dev, O_RDWR);
	if (fd == -1) {
		perror("open");
		exit(1);
	}

	printf("Send command %i\n", cmd);
	send_cmd(fd, cmd);

	if (cmd & ML_FIRE) 
		duration = ML_FIRE_DELAY;
	else if (cmd == ML_UP || cmd == ML_DOWN)
		duration /= 2;
	usleep(duration * 1000);

	send_cmd(fd, ML_STOP);

	close(fd);

	return EXIT_SUCCESS;
}

