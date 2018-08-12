#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <getopt.h>
#include <sys/stat.h>
#include "miscutils.h"
#include "icdi.h"
#include "tm4c123x.h"

static inline int debug_clock(struct icdibuf *buf)
{
	static const char *cmd = "debug clock";

	icdi_qRcmd(buf, cmd);
	return buf->buf[1] == 'O' && buf->buf[2] == 'K';
}

struct flash_spec {
	uint32_t addr;
	uint32_t len;
	int erase;
};

static uint32_t flash_write(const char *binfile, struct icdibuf *buf,
			const struct flash_spec *fspec)
{
	uint32_t addr;
	FILE *fbin;
	int cklen, len;
	char *chunk;

	fbin = fopen(binfile, "rb");
	if (!fbin) {
		fprintf(stderr, "Cannot open file: %s\n", binfile);
		return 0;
	}

	if (fspec->erase && !icdi_flash_erase(buf, 0, 0)) {
		fprintf(stderr, "Cannot erase flash memory!\n");
		return 0;
	}

	len = 0;
	chunk = malloc(FLASH_ERASE_SIZE);
	if (!chunk) {
		fprintf(stderr, "Out of Memory!\n");
		goto exit_10;
	}
	addr = fspec->addr;
	while ((cklen = fread(chunk, 1, FLASH_ERASE_SIZE, fbin))) {
		if (fspec->erase == 0 &&
			!icdi_flash_erase(buf, addr, FLASH_ERASE_SIZE)) {
			fprintf(stderr, "Cannot erase flash at %08X\n", addr);
			break;
		}
		if (!tm4c123_debug_ready(buf)) {
			fprintf(stderr, "Debugger stuck! Chip Locked!\n");
			break;
		}
		if (!icdi_flash_write(buf, addr, chunk, cklen)) {
			fprintf(stderr, "Flash write failed at: %08X\n", addr);
			break;
		}
		addr += cklen;
		len += cklen;
	}
	if (!feof(fbin))
		fprintf(stderr, "Flash operation failed!\n");

	free(chunk);
exit_10:
	fclose(fbin);
	return len;
}

struct cmdargs {
	uint32_t addr, len;
	int erase;
	const char *binfile, *icdi_dev;
};

static int parse_cmdline(struct cmdargs *args, int argc, char *argv[])
{
	static const struct option lopts[] = {
		{.name = "fwbin", .has_arg = required_argument, .flag = NULL, .val = 'f'},
		{.name = "icdi", .has_arg = required_argument, .flag = NULL, .val = 'i'},
		{.name = "addr", .has_arg = required_argument, .flag = NULL, .val = 'a'},
		{.name = "erase", .has_arg = no_argument, .flag = NULL, .val = 'e'},
		{.name = NULL, .has_arg = 0, .flag = 0, .val = 0}
	};
	static const char *opts = "f:i:a:e";
	extern char *optarg;
	extern int optind, opterr, optopt;
	int fin, lidx, optc, retv, sysret;
	struct stat mstat;

	retv = 0;
	optarg = NULL;
	opterr = 0;
	fin = 0;
	args->addr = 0;
	args->erase = 0;
	do {
		optopt = 0;
		lidx = -1;
		optc = getopt_long(argc, argv,  opts, lopts, &lidx);
		if (optarg && *optarg == '-' && optc != 'e') {
			fprintf(stderr, "Missing arguments for ");
			if (lidx == -1)
				fprintf(stderr, "'%c'\n", optc);
			else
				fprintf(stderr, "'%s'\n", lopts[lidx].name);
			optind--;
			continue;
		}
		switch(optc) {
		case -1:
			fin = 1;
			break;
		case '?':
			fprintf(stderr, "Unknown options ");
			if (optopt)
				fprintf(stderr, "'%c'\n", optopt);
			else
				fprintf(stderr, "'%s'\n", argv[optind-1]);
			break;
		case 'f':
			args->binfile = optarg;
			break;
		case 'i':
			args->icdi_dev = optarg;
			break;
		case 'a':
			args->addr = strtol(optarg, NULL, 0);
			break;
		case 'e':
			args->erase = 1;
			break;
		default:
			fprintf(stderr, "Parse options logic error\n");
		}
	} while (fin == 0);

	if ((args->addr % FLASH_ERASE_SIZE) != 0) {
		fprintf(stderr, "Address must be divisible by %d\n",
			FLASH_ERASE_SIZE);
		retv = 4;
	}
	if (args->icdi_dev == NULL) {
		fprintf(stderr, "An ICDI inteface must be specified.\n");
		retv = 8;
	} else {
		sysret = stat(args->icdi_dev, &mstat);
		if (sysret == -1) {
			fprintf(stderr, "Cannot open ICDI device: %s->%s\n",
				args->icdi_dev, strerror(errno));
			retv = 16;
		} else if (!S_ISCHR(mstat.st_mode)) {
			fprintf(stderr, "ICDI device \"%s\" not valid.\n",
				args->icdi_dev);
			retv = 20;
		}
	}
	args->len = 0;
	if (args->binfile == NULL) {
		fprintf(stderr, "A FW binary file must be specified.\n");
		retv = 24;
	} else {
		sysret = stat(args->binfile, &mstat);
		if (sysret == -1 || !S_ISREG(mstat.st_mode)) {
			fprintf(stderr, "File \"%s\" is invalid",
				args->binfile);
			if (sysret == -1)
				fprintf(stderr, ":%s\n", strerror(errno));
			else
				fprintf(stderr, ".\n");
			retv = 28;
		} else  if (mstat.st_size >= 1024*1024*1024ul) {
			fprintf(stderr, "File size too large: %lu\n",
				(unsigned long)mstat.st_size);
			retv = 32;
		} else
			args->len = mstat.st_size;
	}

	return retv;
}

int main(int argc, char *argv[])
{
	struct icdibuf *buf;
	char options[128];
	uint32_t val, did0, did1;
	int retv;
	uint32_t flashsiz;
	struct cmdargs args;
	struct flash_spec fspec;

	if (!instance_start(lock)) {
		fprintf(stderr, "ICDI interface is being locked.\n");
		return 100;
	}
	memset(&args, 0, sizeof(args));
	if ((retv = parse_cmdline(&args, argc, argv)))
		return retv;;
	fspec.addr = args.addr;
	fspec.len = args.len;
	fspec.erase = args.erase;

	buf = icdi_init(args.icdi_dev, FLASH_ERASE_SIZE);
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
	if (!icdi_stop_target(buf))
		fprintf(stderr, "Warning! Target not stopped.\n");

	if (!icdi_readu32(buf, SCSP_BASE+RM_CTRL_OFFSET, &val)) {
		fprintf(stderr, "Cannot read RM_CTRL: %#08x\n",
			SCSP_BASE+RM_CTRL_OFFSET);
		retv = 4;
		goto exit_10;
	}
	if (val & 1) {
		fprintf(stderr, "Flash memory is not mapped at address 0x0\n");
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
	if ((fspec.addr + fspec.len) > flashsiz) {
		fprintf(stderr, "File exceeds Flash Size: %u+%u\n",
			fspec.addr, fspec.len);
		retv = 24;
		goto exit_10;
	}

	if (!tm4c123_debug_ready(buf)) {
		fprintf(stderr, "Micro chip stuck.\n");
		retv = 28;
		goto exit_10;
	}

	flash_write(args.binfile, buf, &fspec);

	printf("Flash finished!\n");
	if (!tm4c123_debug_ready(buf)) {
		fprintf(stderr, "Micro chip stuck.\n");
		retv = 28;
	}
	if (!icdi_chip_reset(buf))
		fprintf(stderr, "Cannot reset the Chip.\n");
	printf("Reset done!\n");
/*	sleep(1);
	if (!tm4c123_debug_ready(buf)) {
		fprintf(stderr, "Micro chip stuck.\n");
		retv = 28;
	}
	icdi_qRcmd(buf, "debug disable"); */
exit_10:
	icdi_exit(buf);
	instance_exit(lock);
	return retv;
}
