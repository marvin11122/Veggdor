#ifndef VEG_HOST_H
#define VEG_HOST_H

#include "veg.h"

#define HOST_FD_MAX 32

typedef struct {
    int used;
    int fd;
} host_fd_t;

typedef struct {
    uint64_t  bump;
    int       exit_code;
    int       exited;
    host_fd_t fds[HOST_FD_MAX];
} veg_host_state;

/* Default host: Alloc/Free/Exit + Open/Close/Read/Write via OS fds. */
int veg_host_default(uint64_t *mem, size_t mem_size, void *userdata);

#endif
