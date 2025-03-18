#include "syscall.h"

#define HEAP_SIZE 65536
static char heap[HEAP_SIZE];
static char* heap_ptr = heap;

// non-reentrant
int _close(int fd) {
    errno = ENOSYS; // Function not implemented
    return -1;
}

int _fstat(int fd, struct stat* st) {
    errno = ENOSYS; // Function not implemented
    return -1;
}

int _getpid(void) {
    errno = ENOSYS; // Function not implemented
    return -1;
}

int _isatty(int fd) {
    errno = ENOSYS; // Function not implemented
    return -1;
}

int _kill(int pid, int sig) {
    errno = ENOSYS; // Function not implemented
    return -1;
}

off_t _lseek(int fd, off_t offset, int whence) {
    errno = ENOSYS; // Function not implemented
    return -1;
}

int _read(int fd, void* buf, size_t count) {
    errno = ENOSYS; // Function not implemented
    return -1;
}

int _write(int fd, const void* buf, size_t count) {
    errno = ENOSYS; // Function not implemented
    return -1;
}
void* _sbrk(ptrdiff_t incr) { return _sbrk_r(NULL, incr); }

void _exit(int status) {
    while (1) {
        // infinite loop
    }
}

// reentrant
int _close_r(struct _reent* ptr, int fd) {
    ptr->_errno = ENOSYS; // Function not implemented
    return -1;
}

int _fstat_r(struct _reent* ptr, int fd, struct stat* st) {
    ptr->_errno = ENOSYS; // Function not implemented
    return -1;
}

int _getpid_r(struct _reent* ptr) {
    ptr->_errno = ENOSYS; // Function not implemented
    return -1;
}

int _isatty_r(struct _reent* ptr, int fd) {
    ptr->_errno = ENOSYS; // Function not implemented
    return -1;
}

int _kill_r(struct _reent* ptr, int pid, int sig) {
    ptr->_errno = ENOSYS; // Function not implemented
    return -1;
}

off_t _lseek_r(struct _reent* ptr, int fd, off_t offset, int whence) {
    ptr->_errno = ENOSYS; // Function not implemented
    return -1;
}

_ssize_t _read_r(struct _reent* ptr, int fd, void* buf, size_t count) {
    ptr->_errno = ENOSYS; // Function not implemented
    return -1;
}

_ssize_t _write_r(struct _reent* ptr, int fd, const void* buf, size_t count) {
    ptr->_errno = ENOSYS; // Function not implemented
    return -1;
}

void* _sbrk_r(struct _reent* r, ptrdiff_t incr) {
    if (heap_ptr + incr > heap + HEAP_SIZE) {
        errno = ENOMEM;
        return (void*)-1;
    }
    void* prev_heap_ptr = heap_ptr;
    heap_ptr += incr;
    return prev_heap_ptr;
}

