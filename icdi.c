#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include "icdi.h"

static int escape(const char *inbuf, int len, char *outbuf)
{
	const char *ibuf;
	char *obuf, cc;

	*outbuf = *inbuf;
	ibuf = inbuf+1;
	obuf = outbuf+1;
	while (ibuf < inbuf + len) {
		cc = *ibuf++;
		if (cc == ESCAPE || cc == START || cc == END) {
			*obuf++ = ESCAPE;
			cc ^= 0x20;
		}
		*obuf++ = cc;
	}
	return obuf - outbuf;
}

static int unescape(const char *inbuf, int len, char *outbuf)
{
	const char *ibuf;
	char *obuf, cc;

	*outbuf = *inbuf;
	ibuf = inbuf + 1;
	obuf = outbuf + 1;
	while (ibuf < inbuf + len) {
		cc = *ibuf++;
		if (cc == ESCAPE)
			cc = *ibuf++ ^ 0x20;
		*obuf++ = cc;
	}
	return obuf - outbuf;
}

static int send_down(struct icdibuf *buf)
{
	int retlen, i, buflen, tries;
	uint8_t sum;
	char echo;

	if (buf->len <= 0)
		return 0;

	buflen = escape(buf->buf, buf->len, buf->wbuf);

	sum = 0;
	for (i = 1; i < buflen; i++)
		sum += buf->wbuf[i];
	buflen += sprintf(buf->wbuf + buflen, "%c%02x", END, sum);

	tries = 0;
	echo = '-';
	do {
		retlen = write(buf->port, buf->wbuf, buflen);
		if (retlen == -1) {
			printf("Error transmitting data %s\n", strerror(errno));
			break;
		}
		read(buf->port, &echo, 1);
		tries++;
	} while (echo != '+' && tries < 5);
	if (echo != '+') {
		fprintf(stderr, "Connection to target is not stable\n");
		retlen = -1;
	}

	return retlen;
}

static int recv_up(struct icdibuf *buf)
{
	int retlen, len, srem;
	char *u;

	retlen = 0;
	srem = BUFSIZE;
	u = buf->wbuf;
	do {
		len = read(buf->port, u, srem);
		if (len == -1) {
			printf("Error receiving data %s\n", strerror(errno));
			break;
		}
		retlen += len;
		u += len;
		srem -= len;
	} while ((retlen < 3) || (buf->wbuf[retlen-3] != '#'));

	buf->len = unescape(buf->wbuf, retlen, buf->buf);

	return buf->len;
}

static int sendrecv(struct icdibuf *buf)
{
	int retlen;

	retlen = send_down(buf);
	if (retlen <= 0)
		return retlen;
	retlen = recv_up(buf);
	return retlen;
}

int icdi_qRcmd(struct icdibuf *buf, const char *cmd)
{
	static const char cmdprefix[] = "qRcmd,";
	int i, idx;
	const char *cstr;

	idx = sprintf(buf->buf, "%c%s", START, cmdprefix);
        for (cstr = cmd, i = 0; i < strlen(cmd); i++)
                idx += sprintf(buf->buf + idx, "%02x", (unsigned int)(*cstr++));
	buf->len = idx;
	idx = sendrecv(buf);
	return idx;
}


static inline void hex2str(const char *hex, int xlen, char *str, int size)
{
	char r, c, *ostr;
	const char *ihex;

	ihex = hex;
	ostr = str;
	for (ihex = hex, ostr = str; ihex < hex + xlen && ostr < str + size;
			ihex++, ostr++) {
		r = *ihex++;
		c = (r <= '9') ? (r - '0') << 4 : (r - 'a' + 10) << 4;
		r = *ihex;
		c |= (r <= '9') ? r - '0' : r - 'a' + 10;
		*ostr = c;
	}
	if (ostr < str + size)
		*ostr = 0;
	else
		*(ostr+size-1) = 0;
}

static inline void u32_le2cpu(uint32_t *val)
{
}
static inline void u32_cpu2le(uint32_t *val)
{
}

static inline int sendstr(struct icdibuf *buf, const char *str)
{
	int idx;

	idx = sprintf(buf->buf, "%c%s", START, str);
	buf->len = idx;
	idx = sendrecv(buf);
	return idx;
}

int icdi_qSupported(struct icdibuf *buf, char *options, int len)
{
	int size;

	size = sendstr(buf, "qSupported");
	size = size > len? len : size-4;
	memcpy(options, buf->buf+1, size);
	options[size] = 0;
	
	return size;
}

int icdi_version(struct icdibuf *buf, char *ver, int len)
{
	int xlen;
	static const char *cmd = "version";

	*ver = 0;
	xlen = icdi_qRcmd(buf, cmd);
	if (xlen > 4)
		hex2str(buf->buf+1, xlen-4, ver, len);
	return xlen - 4;
}

struct icdibuf *icdi_init(const char *serial_port)
{
	struct icdibuf *buf;
	int port, sysret;
	struct termios ctltio;

	port = open(serial_port, O_RDWR|O_NOCTTY);
	if (port == -1) {
		fprintf(stderr, "Cannot open \"%s\"->%s\n", serial_port, strerror(errno));
		return NULL;
	}

	bzero(&ctltio, sizeof(ctltio));
	ctltio.c_iflag = IGNBRK;
	ctltio.c_cflag = CS8 | CLOCAL | CREAD;
	ctltio.c_cc[VMIN] = 1;
	ctltio.c_cc[VTIME] = 2;
	sysret = tcsetattr(port, TCSANOW, &ctltio);
	if (sysret == -1) {
		fprintf(stderr, "tcgetattr failed for %s: %s\n", serial_port, strerror(errno));
		return NULL;
	}
	buf = malloc(sizeof(struct icdibuf));
	if (!buf)
		fprintf(stderr, "Out of Memory!\n");
	else {
		buf->port = port;
		buf->len = 0;
	}
	return buf;
}

int icdi_readu32(struct icdibuf *buf, uint32_t addr, uint32_t *val)
{
	buf->len = sprintf(buf->buf, "%cx%08x,4", START, addr);
	sendrecv(buf);
	*val = buf->bdat.u32[0];
	return buf->bdat.O == 'O' && buf->bdat.K == 'K';
}

int icdi_readbin(struct icdibuf *buf, uint32_t addr, int len, char *binstr)
{
	int rlen;

	if (len > BUFSIZ - 128) {
		fprintf(stderr, "chunk too large: %d -- %d\n", len, BUFSIZ);
		return 0;
	}

	buf->len = sprintf(buf->buf, "%cx%08x,%x", START, addr, len);
	rlen = sendrecv(buf);
	if (rlen <= 0 || buf->bdat.O != 'O' || buf->bdat.K != 'K') {
		fprintf(stderr, "Memory read failed\n");
		return 0;
	}
	memcpy(binstr, buf->buf+4, buf->len-7);
	return buf->len-7;
}

int icdi_writeu32(struct icdibuf *buf, uint32_t addr, uint32_t val)
{
	uint32_t rval;

	rval = val;
	buf->len = sprintf(buf->buf, "%cX%08x,4:", START, addr);
	u32_cpu2le(&rval);
	memcpy(buf->buf+buf->len, &rval, sizeof(val));
	buf->len += sizeof(val);
	sendrecv(buf);
	return buf->bdat.O == 'O' && buf->bdat.K == 'K';
}

int icdi_stop_target(struct icdibuf *buf)
{
	int idx;

	buf->len = sprintf(buf->buf, "%c?", START);
	idx = sendrecv(buf);
	return buf->bdat.O == 'S' && idx == 7;
}

int icdi_flash_erase(struct icdibuf *buf, uint32_t addr, int len)
{
	buf->len = sprintf(buf->buf, "%cvFlashErase:%08x,%08x", START, addr, len);
	sendrecv(buf);
	return buf->bdat.O == 'O' && buf->bdat.K == 'K';
}

int icdi_flash_write(struct icdibuf *buf, uint32_t addr, char *binstr, int len)
{
	int idx;

	idx = sprintf(buf->buf, "%cvFlashWrite:%08x:", START, addr);
	memcpy(buf->buf+idx, binstr, len);
	buf->len = idx + len;
	sendrecv(buf);
	return buf->bdat.O == 'O' && buf->bdat.K == 'K';
}
