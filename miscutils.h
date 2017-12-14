#ifndef MISCUTILS_DSCAO__
#define MISCUTILS_DSCAO__
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

static inline int instance_start(const char *file_lock)
{
	int fd;
	char fname[64];

	strcpy(fname, "/var/lock/");
	strcat(fname, file_lock);
	fd = open(fname, O_WRONLY|O_CREAT|O_EXCL);
	if (fd == -1)
		return 0;
	close(fd);
	return 1;
}

static inline void instance_exit(const char *file_lock)
{
	char fname[64];

	strcpy(fname, "/var/lock/");
	strcat(fname, file_lock);
	unlink(fname);
}
#endif /* MISCUTILS_DSCAO__ */
