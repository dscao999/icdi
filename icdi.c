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
	char *obuf, cc, pc;
	int repeat, i;

	*outbuf = *inbuf;
	ibuf = inbuf + 1;
	obuf = outbuf + 1;
	pc = 0;
	while (ibuf < inbuf + len) {
		cc = *ibuf++;
		if (cc == ESCAPE)
			cc = *ibuf++ ^ 0x20;
		else if (cc == STAR) {
			cc = pc;
			repeat = *ibuf++ - 29;
			for (i = 0; i < repeat - 1; i++)
				*obuf++ = cc;
		}
		*obuf++ = cc;
		pc = cc;
	}
	return obuf - outbuf;
}

static int send_down(struct icdibuf *buf, int size, int binary)
{
	int retlen, i, buflen;
	uint8_t sum;

	if (size <= 0)
		return 0;

	buflen = size;
	if (binary)
		memcpy(buf->wbuf, buf->buf, size);
	else
		buflen = escape(buf->buf, size, buf->wbuf);

	sum = 0;
	for (i = 1; i < buflen; i++)
		sum += buf->wbuf[i];
	buflen += sprintf(buf->wbuf + buflen, "%c%02x", END, sum);

	retlen = write(buf->port, buf->wbuf, buflen);
	if (retlen != buflen)
		printf("Error transmitting data %s\n", strerror(errno));
	return retlen;
}

static int recv_up(struct icdibuf *buf, int *has_ack, int binary)
{
	int retlen, len, srem, size;
	char *u;

	*has_ack = 0;
	retlen = 0;
	srem = BUFSIZE;
	u = buf->wbuf;
	do {
		len = read(buf->port, u, srem);
		if (len == -1) {
			printf("Error receiving data %s\n", strerror(errno));
			break;
		}
		if (len >= 1 && buf->wbuf[0] == '+')
			*has_ack = 1;
		retlen += len;
		u += len;
		srem -= len;
	} while ((retlen < 3) || (buf->wbuf[retlen-3] != '#'));

	size = retlen;
	if (binary)
		memcpy(buf->buf, buf->wbuf, retlen);
	else
		size = unescape(buf->wbuf, retlen, buf->buf);

	return size;
}

static int sendrecv(struct icdibuf *buf, int size, int binary)
{
	int retlen, has_ack;

	if (size + END_LEN > BUFSIZE) {
		fprintf(stderr, "Output buffer overflow!\n");
		return 0;
	}

	retlen = send_down(buf, size, binary);
	if (retlen <= 0)
		return retlen;
	retlen = recv_up(buf, &has_ack, binary);
	if (retlen <= 0)
		return retlen;
	if (!has_ack)
		retlen = 0;

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

	idx = sendrecv(buf, idx, 0);
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
	idx = sendrecv(buf, idx, 0);
	return idx;
}

int icdi_qSupported(struct icdibuf *buf, char *options, int len)
{
	int size;

	size = sendstr(buf, "qSupported");
	size = size > len? len : size;
	memcpy(options, buf->buf, size);
	options[size] = 0;
	
	return size;
}

void icdi_version(struct icdibuf *buf, char *ver, int len)
{
	int xlen;
	static const char *cmd = "version";

	*ver = 0;
	xlen = icdi_qRcmd(buf, cmd);
	if (xlen >= 5)
		hex2str(buf->buf+2, xlen-5, ver, len);
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
	else
		buf->port = port;
	return buf;
}

int icdi_readu32(struct icdibuf *buf, uint32_t addr, uint32_t *val)
{
	int idx;

	idx = sprintf(buf->buf, "%cx%08x,4", START, addr);
	idx = sendrecv(buf, idx, 1);
	*val = buf->bdat.u32[0];
	return buf->bdat.O == 'O' && buf->bdat.K == 'K';
}

int icdi_writeu32(struct icdibuf *buf, uint32_t addr, uint32_t val)
{
	int idx;
	uint32_t rval;

	rval = val;
	idx = sprintf(buf->buf, "%cX%08x,4:", START, addr);
	u32_cpu2le(&rval);
	memcpy(buf->buf+idx, &rval, sizeof(val));
	idx += sizeof(val);
	idx = sendrecv(buf, idx, 1);
	return buf->bdat.O == 'O' && buf->bdat.K == 'K';
}

int icdi_stop_target(struct icdibuf *buf)
{
	int idx;

	idx = sprintf(buf->buf, "%c?", START);
	idx = sendrecv(buf, idx, 0);
	return buf->bdat.O == 'S' && idx == 8;
}
