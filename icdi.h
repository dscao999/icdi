#ifndef ICDI_DSCAO__
#define ICDI_DSCAO__
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>

#define FLASH_BLOCK_SIZE 512
#define FLASH_ERASE_SIZE 1024
/* Prefix + potentially every flash byte escaped */
#define BUFSIZE (64 + 2*FLASH_BLOCK_SIZE)

#define START	'$'
#define END	'#'
#define STAR	'*'
#define ESCAPE	'}'

#define START_LEN	1
#define END_LEN		3 

struct bindat {
	char plus;
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
void icdi_version(struct icdibuf *buf, char *ver, int len);
int icdi_qSupported(struct icdibuf *buf, char *options, int len);
int icdi_readu32(struct icdibuf *buf, uint32_t addr, uint32_t *val);
int icdi_writeu32(struct icdibuf *buf, uint32_t addr, uint32_t val);
int icdi_stop_target(struct icdibuf *buf);

static inline int icdi_debug_cmd(struct icdibuf *buf, const char *cmd)
{
	icdi_qRcmd(buf, 
}

#endif /* ICDI_DSCAO__ */
