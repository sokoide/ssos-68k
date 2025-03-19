#include "syscall.h"

extern const void* __bss_end;

#define HEAP_SIZE 65536
/* static char heap[HEAP_SIZE]; */
/* static char* heap_ptr = heap; */

uint8_t* heap_end = (uint8_t*)&__bss_end;
uint8_t* prev_heap_end;

// non-reentrant
int _close(int fd) { return 0; }

int _fstat(int fd, struct stat* st) { return 0; }

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

off_t _lseek(int fd, off_t offset, int whence) { return 0; }

int _read(int fd, void* buf, size_t count) { return 0; }

int _write(int fd, const void* buf, size_t count) { return 0; }

void* _sbrk(ptrdiff_t incr) { return _sbrk_r(NULL, incr); }

void _exit(int status) {
    while (1) {
        // infinite loop
    }
}

// reentrant
int _close_r(struct _reent* ptr, int fd) { return 0; }

int _fstat_r(struct _reent* ptr, int fd, struct stat* st) { return 0; }

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
    return 0;
}

_ssize_t _read_r(struct _reent* ptr, int fd, void* buf, size_t count) {
    return 0;
}

_ssize_t _write_r(struct _reent* ptr, int fd, const void* buf, size_t count) {
    return 0;
}

void* _sbrk_r(struct _reent* r, ptrdiff_t incr) {
    /* if (heap_ptr + incr > heap + HEAP_SIZE) { */
    /*     errno = ENOMEM; */
    /*     return (void*)-1; */
    /* } */
    /* void* prev_heap_ptr = heap_ptr; */
    /* heap_ptr += incr; */
    /* return prev_heap_ptr; */
#define TASK_STACK_UPPER_LIMIT 0x0080000000

    prev_heap_end = heap_end;
    if ((heap_end + incr) > (uint8_t*)TASK_STACK_UPPER_LIMIT)
        return (void*)-1;
    heap_end += incr;
    return prev_heap_end;
}

