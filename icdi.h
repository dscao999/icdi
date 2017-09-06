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

struct commbuf {
	int port;
	union {
		char c[BUFSIZE];
		uint8_t u8[BUFSIZE];
		uint32_t u32[BUFSIZE / 4];
	};
	char buf[BUFSIZE];
};

struct commbuf *commbuf_init(const char *serial_port);
static inline void commbuf_exit(struct commbuf *buf)
{
	close(buf->port);
	free(buf);
};

int send_qRcmd(struct commbuf *buf, const char *cmd);
void print_icdi_version(struct commbuf *buf);
int qSupported(struct commbuf *buf, char *options, int len);

#endif /* ICDI_DSCAO__ */
