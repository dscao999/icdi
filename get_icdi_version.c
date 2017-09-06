#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "icdi.h"

static int debug_clock(struct commbuf *buf)
{
	static const char *cmd = "debug clock";

	send_qRcmd(buf, cmd);
	return buf->c[2] == 'O' && buf->c[3] == 'K';
}

int main(int argc, char *argv[])
{
	struct commbuf *buf;
	char options[128];

	if (argc < 2) {
		fprintf(stderr, "The ICDI port name must be specified.\n");
		return 8;
	}

	buf = commbuf_init(argv[1]);
	if (buf == NULL) {
		fprintf(stderr, "Out of Memory!\n");
		return 10000;
	}

	print_icdi_version(buf);
	if (debug_clock(buf))
		printf("Debug Clock OK!\n");
	else
		printf("Debug Clock Unstable!\n");
	if (qSupported(buf, options, 128))
		printf("%s\n", options);
	if (qSupported(buf, options, 128))
		printf("%s\n", options);

	commbuf_exit(buf);
	return 0;
}
