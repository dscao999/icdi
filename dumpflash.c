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

struct flash_spec {
	uint32_t addr;
	uint32_t len;
};

static uint32_t flash_dump(const char *binfile, struct icdibuf *buf,
			const struct flash_spec *fspec)
{
	uint32_t len, addr;
	FILE *fbin;
	int err, cklen;
	char *chunk;

	fbin = fopen(binfile, "wb");
	if (!fbin) {
		fprintf(stderr, "Cannot open file: %s\n", binfile);
		return 0;
	}

	chunk = malloc(FLASH_BLOCK_SIZE);
	err = 0;
	addr = fspec->addr;
	len = 0;
	do {
		if (!tm4c123_debug_ready(buf)) {
			fprintf(stderr, "Micro Chip got stuck!\n");
			goto exit_10;
		}
		cklen = icdi_readbin(buf, addr, FLASH_BLOCK_SIZE, chunk);
		if (cklen != FLASH_BLOCK_SIZE) {
			err = 1;
			fprintf(stderr, "Flash read error!\n");
		}
		fwrite(chunk, 1, cklen, fbin);
		len += cklen;
		addr += cklen;
	} while (len < fspec->len && !err);

exit_10:
	free(chunk);
	fclose(fbin);
	return len;
}

struct cmdargs {
	uint32_t addr, len;
	const char *binfile, *icdi_dev;
};

static int parse_cmdline(struct cmdargs *args, int argc, char *argv[])
{
	static const struct option lopts[] = {
		{.name = "output", .has_arg = required_argument, .flag = NULL, .val = 'o'},
		{.name = "icdi", .has_arg = required_argument, .flag = NULL, .val = 'i'},
		{.name = "addr", .has_arg = required_argument, .flag = NULL, .val = 'a'},
		{.name = "length", .has_arg = required_argument, .flag = NULL, .val = 'l'},
		{.name = NULL, .has_arg = 0, .flag = 0, .val = 0}
	};
	static const char *opts = "o:i:a:l:";
	extern char *optarg;
	extern int optind, opterr, optopt;
	int fin, lidx, optc, retv, sysret;
	char *suffix;
	uint32_t len;
	struct stat mstat;

	retv = 0;
	optarg = NULL;
	opterr = 0;
	fin = 0;
	args->addr = 0;
	len = 0;
	do {
		optopt = 0;
		lidx = -1;
		optc = getopt_long(argc, argv,  opts, lopts, &lidx);
		if (optarg && *optarg == '-' &&
			(optc == 'o' || optc == 'i')) {
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
		case 'o':
			args->binfile = optarg;
			break;
		case 'i':
			args->icdi_dev = optarg;
			break;
		case 'a':
			args->addr = strtol(optarg, NULL, 0);
			break;
		case 'l':
			len = strtol(optarg, &suffix, 0);
			switch(*suffix) {
			case 'M':
			case 'm':
				len *= 1024;
			case 'K':
			case 'k':
				len *= 1024;
			}
			break;
		default:
			fprintf(stderr, "Parse options logic error\n");
		}
	} while (fin == 0);

	if (len != 0)
		args->len = ((len-1)/FLASH_BLOCK_SIZE+1)*FLASH_BLOCK_SIZE;
	if ((args->addr % FLASH_BLOCK_SIZE) != 0) {
		fprintf(stderr, "Address must be divisible by %d\n",
			FLASH_BLOCK_SIZE);
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
	if (args->binfile == NULL) {
		args->binfile = "/tmp/tivac.bin";
		fprintf(stderr, "BIN file set to \"/tmp/tivac.bin\"\n");
	}
	sysret = stat(args->binfile, &mstat);
	if (sysret == 0 && !S_ISREG(mstat.st_mode)) {
		fprintf(stderr, "File \"%s\" is not a regular file\n",
			args->binfile);
		retv = 28;
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
		fprintf(stderr, "ICDI port is being locked.\n");
		return 100;
	}	
	memset(&args, 0, sizeof(args));
	if ((retv = parse_cmdline(&args, argc, argv)))
		return retv;;
	fspec.addr = args.addr;
	fspec.len = args.len;

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
	if (fspec.len == 0)
		fspec.len = flashsiz;

	flash_dump(args.binfile, buf, &fspec);

	if (!icdi_chip_reset(buf))
		fprintf(stderr, "Failed to reset the chip.\n");
	icdi_qRcmd(buf, "debug disable");
exit_10:
	icdi_exit(buf);
	instance_exit(lock);
	return retv;
}
