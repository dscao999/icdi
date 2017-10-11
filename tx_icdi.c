#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include "icdi.h"
#include "tm4c123x.h"

static inline int debug_clock(struct icdibuf *buf)
{
	static const char *cmd = "debug clock";

	icdi_qRcmd(buf, cmd);
	return buf->buf[1] == 'O' && buf->buf[2] == 'K';
}

int main(int argc, char *argv[])
{
	struct icdibuf *buf;
	char options[128];
	uint32_t val, did0, did1;
	int retv;
	uint32_t flashsiz, en0, pri0, stctrl;

	if (argc < 2) {
		fprintf(stderr, "The ICDI port name must be specified.\n");
		return 8;
	}

	buf = icdi_init(argv[1], FLASH_ERASE_SIZE);
	if (buf == NULL)
		return 1000;

	retv = 0;
	icdi_version(buf, options, 128);
	printf("ICDI Version: %s", options);
	if (icdi_qSupported(buf, options, 128))
		printf("Supported: %s\n", options);

	if (!debug_clock(buf)) {
		fprintf(stderr, "Debug Clock is not stable!\n");
		retv = 100;
		goto exit_10;
	}

	if (!icdi_stop_target(buf)) {
		fprintf(stderr, "Cannot stop target.\n");
		retv = 104;
		goto exit_10;
	}

	if (!icdi_readu32(buf, SCSP_BASE+RM_CTRL_OFFSET, &val)) {
		fprintf(stderr, "Cannot read RM_CTRL: %#08x\n",
			SCSP_BASE+RM_CTRL_OFFSET);
		retv = 4;
		goto exit_10;
	}
	if (val & 1) {
		fprintf(stderr, "Internal ROM is mapped at address 0x0\n");
		retv = 8;
		goto exit_10;
	}
	if (!icdi_readu32(buf, SCSP_BASE+DID0_OFFSET, &did0) ||
		!icdi_readu32(buf, SCSP_BASE+DID1_OFFSET, &did1)) {
		fprintf(stderr, "Cannot read DID0/DID1.\n");
		retv = 12;
		goto exit_10;
	}
	
	if (((did0 >> 16) & 0x0ff) == 0x05)
		printf("TM4C123x Chip, ");
	else
		printf("Unsupported Chip\n");
	if (((did1 >> 16) & 0x0ff) == 0x0A1)
		printf("TM4C123GH6PM microcontroller.\n");
	printf("DID0: %08X, DID1: %08X\n", did0, did1);

	if (!icdi_readu32(buf, FM_CTRL_BASE+FSIZE_OFFSET, &flashsiz)) {
		fprintf(stderr, "Cannot get flash memory size.\n");
		retv = 16;
		goto exit_10;
	}
	if (flashsiz == 0x7f)
		flashsiz = 256*1024;
	else {
		fprintf(stderr, "Unknown flash size.\n");
		retv = 20;
		goto exit_10;
	}
	printf("Flash Size: %dKiB\n", flashsiz/1024);
	if (!icdi_readu32(buf, SCSS_BASE+SCSS_EN0_OFFSET, &en0))
		fprintf(stderr, "Cannot read Interrupt enable reg 0.\n");
	else
		printf("Interrupt Enable Register 0: %08X\n", en0);
	if (!icdi_readu32(buf, SCSS_BASE+SCSS_PRI0_OFFSET, &pri0))
		fprintf(stderr, "Cannot read Interrupt Priority Reg 0.\n");
	else
		printf("Interrupt Priority Register 0: %08X\n", pri0);

	if (!icdi_readu32(buf, SCSS_BASE+SCSS_STCTRL_OFFSET, &stctrl))
		fprintf(stderr, "Cannot read SysTick Control Register.\n");
	else
		printf("SysTick Control Register: %08X\n", pri0);

	icdi_qRcmd(buf, "debug disable");
exit_10:
	icdi_exit(buf);
	return retv;
}
