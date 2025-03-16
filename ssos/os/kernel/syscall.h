#pragma onde

#include <errno.h>
#include <reent.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>

int _close(int fd);
int _fstat(int fd, struct stat* st);
int _getpid(void);
int _isatty(int fd);
int _kill(int pid, int sig);
off_t _lseek(int fd, off_t offset, int whence);
int _read(int fd, void* buf, size_t count);
int _write(int fd, const void* buf, size_t count);
void* _sbrk(ptrdiff_t incr);
void _exit(int status);

int _close_r(struct _reent* ptr, int fd);
int _fstat_r(struct _reent* ptr, int fd, struct stat* st);
int _getpid_r(struct _reent* ptr);
int _isatty_r(struct _reent* ptr, int fd);
int _kill_r(struct _reent* ptr, int pid, int sig);
off_t _lseek_r(struct _reent* ptr, int fd, off_t offset, int whence);
_ssize_t _read_r(struct _reent* ptr, int fd, void* buf, size_t count);
_ssize_t _write_r(struct _reent* ptr, int fd, const void* buf, size_t count);
void* _sbrk_r(struct _reent* r, ptrdiff_t incr);
