#ifndef ICDI_DSCAO__
#define ICDI_DSCAO__
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>

#define FLASH_BLOCK_SIZE 512
#define FLASH_ERASE_SIZE 1024
/* Prefix + potentially every flash byte escaped */
#define BUFSIZE 2176  /* 128 + 2048 */

#define START	'$'
#define END	'#'
#define STAR	'*'
#define ESCAPE	'}'

#define START_LEN	1
#define END_LEN		3 

struct bindat {
	char dollar;
	char O;
	char K;
	char colon;
	union {
		char c[0];
		uint8_t u8[0];
		uint32_t u32[0];
	};
} __attribute__((packed));
struct icdibuf {
	int port;
	int len;
	union {
		char buf[BUFSIZE];
		struct bindat bdat;
	};
	char wbuf[BUFSIZE];
};

struct icdibuf *icdi_init(const char *serial_port);
static inline void icdi_exit(struct icdibuf *buf)
{
	close(buf->port);
	free(buf);
};

int icdi_qRcmd(struct icdibuf *buf, const char *cmd);
int icdi_version(struct icdibuf *buf, char *ver, int len);
int icdi_qSupported(struct icdibuf *buf, char *options, int len);
static inline int icdi_debug_sreset(struct icdibuf *buf)
{
	return icdi_qRcmd(buf, "debug sreset");
}
static inline int icdi_debug_creset(struct icdibuf *buf)
{
	return icdi_qRcmd(buf, "debug creset");
}
static inline int icdi_chip_reset(struct icdibuf *buf)
{
	if (icdi_qRcmd(buf, "set vectorcatch 0") &&
		icdi_qRcmd(buf, "debug disable"))
		return 1;
	else
		return 0;
}

int icdi_readu32(struct icdibuf *buf, uint32_t addr, uint32_t *val);
int icdi_writeu32(struct icdibuf *buf, uint32_t addr, uint32_t val);

int icdi_readbin(struct icdibuf *buf, uint32_t addr, int len, char *binstr);
int icdi_flash_write(struct icdibuf *buf, uint32_t addr, char *binstr, int len);
int icdi_flash_erase(struct icdibuf *buf, uint32_t addr, int len);

int icdi_stop_target(struct icdibuf *buf);
#endif /* ICDI_DSCAO__ */
