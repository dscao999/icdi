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

static uint32_t flash_read(const char *binfile, struct icdibuf *buf,
			uint32_t fsiz)
{
	uint32_t len, addr;
	FILE *fbin;
	int err, cklen;
	char *chunk;

	assert(fsiz % 1024 == 0);
	fbin = fopen(binfile, "wb");
	if (!fbin) {
		fprintf(stderr, "Cannot open file: %s\n", binfile);
		return 0;
	}

	chunk = malloc(1024);
	err = 0;
	addr = 0;
	len = 0;
	do {
		cklen = icdi_readbin(buf, addr, 1024, chunk);
		if (cklen != 1024) {
			err = 1;
			fprintf(stderr, "Flash read error!\n");
		}
		fwrite(chunk, 1, cklen, fbin);
		len += cklen;
		addr += cklen;
	} while (len < fsiz && !err);

	free(chunk);
	fclose(fbin);
	return len;
}

int main(int argc, char *argv[])
{
	struct icdibuf *buf;
	char options[128];
	uint32_t val, did0, did1;
	int retv;
	uint32_t flashsiz;

	if (argc < 2) {
		fprintf(stderr, "The ICDI port name must be specified.\n");
		return 8;
	}

	buf = icdi_init(argv[1]);
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

	flash_read("/tmp/tm123x.bin", buf, flashsiz);
exit_10:
	icdi_exit(buf);
	return retv;
}
