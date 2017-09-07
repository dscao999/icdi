#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "icdi.h"
#include "tm4c.h"

static int debug_clock(struct icdibuf *buf)
{
	static const char *cmd = "debug clock";

	icdi_qRcmd(buf, cmd);
	return buf->buf[2] == 'O' && buf->buf[3] == 'K';
}

int main(int argc, char *argv[])
{
	struct icdibuf *buf;
	char options[128];
	uint32_t val;

	if (argc < 2) {
		fprintf(stderr, "The ICDI port name must be specified.\n");
		return 8;
	}

	buf = icdi_init(argv[1]);
	if (buf == NULL)
		return 1000;

	icdi_version(buf, options, 128);
	printf("ICDI Version: %s", options);
	if (debug_clock(buf))
		printf("Debug Clock OK!\n");
	else
		printf("Debug Clock Unstable!\n");
	if (icdi_qSupported(buf, options, 128))
		printf("%s\n", options);

/*	if (icdi_stop_target(buf))
		printf("Target stopped!\n"); */

	if (icdi_readu32(buf, FP_CTRL, &val))
		printf("Value at %08X: %08X\n", FP_CTRL, val);
	if (icdi_readu32(buf, SC_BASE+RM_CTRL_OFFSET, &val))
		printf("Value at %08X: %08X\n", SC_BASE+RM_CTRL_OFFSET, val);

	icdi_exit(buf);
	return 0;
}
