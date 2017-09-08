#ifndef TM4C123X_DSCAO__
#define TM4C123X_DSCAO__
#include <time.h>
#include "icdi.h"
/*
 * Memory Addresses of TM4C123x control (system and periperal)
 */
#define FM_CTRL_BASE	0x400fd000
#define FSIZE_OFFSET	0x0fc0

#define SCSP_BASE	0x400fe000
#define DID0_OFFSET	0x0
#define DID1_OFFSET	0x4
#define RCC_OFFSET	0x060
#define RM_CTRL_OFFSET	0x0f0

#define SCSS_BASE	0xe000e000

#define DHCSR		0xe000edf0
#define DHCSR_S_LOCKUP	(1<<19)
#define DHCSR_S_SLEEP	(1<<18)
#define DHCSR_S_HALT	(1<<17)
#define DHCSR_S_REGRDY	(1<<16)

#define FP_CTRL		0xe0002000

static inline int tm4c123_ready(struct icdibuf *buf)
{
	uint32_t dhcsr;
	int count;
	struct timespec sl;

	sl.tv_sec = 0;
	sl.tv_nsec = 100000000;
	count = 0;
	do {
		dhcsr = 0;
		icdi_readu32(buf, DHCSR, &dhcsr);
		if ((dhcsr & DHCSR_S_HALT) && (dhcsr & DHCSR_S_REGRDY))
			break;
		nanosleep(&sl, NULL);
		count++;
	} while (count < 10);
	return count < 10;
};
#endif /* TM4C123X_DSCAO__ */
