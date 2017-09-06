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

static int send_down(struct commbuf *buf, int size, int binary)
{
	int retlen, i, buflen;
	uint8_t sum;

	if (size <= 0)
		return 0;

	buflen = size;
	if (binary)
		memcpy(buf->buf, buf->c, size);
	else
		buflen = escape(buf->c, size, buf->buf);

	sum = 0;
	for (i = 1; i < buflen; i++)
		sum += buf->buf[i];
	buflen += sprintf(buf->buf + buflen, "%c%02x", END, sum);

	retlen = write(buf->port, buf->buf, buflen);
	if (retlen != buflen)
		printf("Error transmitting data %s\n", strerror(errno));
	return retlen;
}

static int recv_up(struct commbuf *buf, int *has_ack, int binary)
{
	int retlen, len, srem, size;
	char *u;

	*has_ack = 0;
	retlen = 0;
	srem = BUFSIZE;
	u = buf->buf;
	do {
		len = read(buf->port, u, srem);
		if (len == -1) {
			printf("Error receiving data %s\n", strerror(errno));
			break;
		}
		if (len >= 1 && buf->buf[0] == '+')
			*has_ack = 1;
		retlen += len;
		u += len;
		srem -= len;
	} while ((retlen < 3) || (buf->buf[retlen-3] != '#'));

	size = retlen;
	if (binary)
		memcpy(buf->c, buf->buf, retlen);
	else
		size = unescape(buf->buf, retlen, buf->c);

	return size;
}

static int sendrecv(struct commbuf *buf, int size, int binary)
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

int send_qRcmd(struct commbuf *buf, const char *cmd)
{
	static const char cmdprefix[] = "qRcmd,";
	int i, idx;
	const char *cstr;

	idx = sprintf(buf->c, "%c%s", START, cmdprefix);
        for (cstr = cmd, i = 0; i < strlen(cmd); i++)
                idx += sprintf(buf->c + idx, "%02x", (unsigned int)(*cstr++));

	idx = sendrecv(buf, idx, 0);
	return idx;
}


void print_echo(const struct commbuf *buf, int xlen)
{
	int i;
	char r, c;

	if (strncmp(buf->c, "+$", 2) != 0)
		return;

	for (i = strlen("+$"); buf->u8[i] != '#'; i += 2) {
		r = buf->u8[i];
		c = (r <= '9') ? (r - '0') << 4 : (r - 'a' + 10) << 4;
		r = buf->u8[i+1];
		c |= (r <= '9') ? r - '0' : r - 'a' + 10;
		printf("%c", c);
	}
}

static inline int sendstr(struct commbuf *buf, const char *str)
{
	int idx;

	idx = sprintf(buf->c, "%c%s", START, str);
	idx = sendrecv(buf, idx, 0);
	return idx;
}

int qSupported(struct commbuf *buf, char *options, int len)
{
	int size, idx;

	size = sendstr(buf, "qSupported");
	memcpy(options, buf->c, size);
	options[size] = 0;
	
	return size;
}

void print_icdi_version(struct commbuf *buf)
{
	int xlen;
	static const char *cmd = "version";

	xlen = send_qRcmd(buf, cmd);
	if (xlen <= 0)
		return;

	printf("ICDI version: ");
	print_echo(buf, xlen);
}

struct commbuf *commbuf_init(const char *serial_port)
{
	struct commbuf *buf;
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
	buf = malloc(sizeof(struct commbuf));
	buf->port = port;
	return buf;
}
