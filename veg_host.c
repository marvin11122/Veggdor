/*
 * veg_host.c — dense host for V_EFFECT
 *
 * Capability enters only here. Core stays pure.
 * fd table + bump heap + exit. No framework.
 */

#include "veg.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HOST_HEAP_START  0x8000ull
#define HOST_HEAP_END    0xFD00ull
#define HOST_FD_MAX      32
#define HOST_PATH_MAX    512

typedef struct {
    int used;
    int fd;       /* OS file descriptor */
} host_fd_t;

typedef struct {
    uint64_t  bump;
    int       exit_code;
    int       exited;
    host_fd_t fds[HOST_FD_MAX];
} veg_host_state;

static int host_fd_alloc(veg_host_state *st, int os_fd)
{
    for (int i = 0; i < HOST_FD_MAX; i++) {
        if (!st->fds[i].used) {
            st->fds[i].used = 1;
            st->fds[i].fd = os_fd;
            return i;
        }
    }
    return -1;
}

static int host_fd_get(veg_host_state *st, uint64_t idx, int *os_fd)
{
    if (idx >= HOST_FD_MAX || !st->fds[idx].used) return -1;
    *os_fd = st->fds[idx].fd;
    return 0;
}

static void host_fd_free(veg_host_state *st, uint64_t idx)
{
    if (idx < HOST_FD_MAX) {
        st->fds[idx].used = 0;
        st->fds[idx].fd = -1;
    }
}

/* Read a path string from lanes: each lane holds one byte in the low 8 bits. */
static int path_from_lanes(const uint64_t *mem, size_t mem_size,
                           uint64_t base, uint64_t len,
                           char *out, size_t out_cap)
{
    if (len == 0 || len >= out_cap || base + len > mem_size) return -1;
    for (uint64_t i = 0; i < len; i++)
        out[i] = (char)(mem[base + i] & 0xff);
    out[len] = '\0';
    return 0;
}

int veg_host_default(uint64_t *mem, size_t mem_size, void *userdata)
{
    veg_host_state *st = (veg_host_state *)userdata;
    if (!mem || !st || VEG_EFFECT_STATUS >= mem_size) return -1;

    uint64_t op = mem[VEG_EFFECT_OP];
    uint64_t a0 = mem[VEG_EFFECT_ARG0];
    uint64_t a1 = mem[VEG_EFFECT_ARG1];
    uint64_t a2 = mem[VEG_EFFECT_ARG2];

    switch (op) {
    case VEG_EFF_NOP:
        mem[VEG_EFFECT_RESULT] = 0;
        mem[VEG_EFFECT_STATUS] = VEG_EFF_DONE;
        return 0;

    case VEG_EFF_ALLOC: {
        if (st->bump == 0) st->bump = HOST_HEAP_START;
        uint64_t n = a0 ? a0 : 1;
        if (st->bump + n > HOST_HEAP_END) {
            mem[VEG_EFFECT_RESULT] = 0;
            mem[VEG_EFFECT_STATUS] = VEG_EFF_ERROR;
            return 0;
        }
        mem[VEG_EFFECT_RESULT] = st->bump;
        st->bump += n;
        mem[VEG_EFFECT_STATUS] = VEG_EFF_DONE;
        return 0;
    }

    case VEG_EFF_FREE:
        mem[VEG_EFFECT_RESULT] = 0;
        mem[VEG_EFFECT_STATUS] = VEG_EFF_DONE;
        return 0;

    case VEG_EFF_EXIT:
        st->exit_code = (int)a0;
        st->exited = 1;
        mem[VEG_EFFECT_RESULT] = a0;
        mem[VEG_EFFECT_STATUS] = VEG_EFF_DONE;
        return 1;

    case VEG_EFF_OPEN: {
        /* a0 = path_base, a1 = path_len, a2 = mode (0=ro, 1=rw, 2=create) */
        char path[HOST_PATH_MAX];
        if (path_from_lanes(mem, mem_size, a0, a1, path, sizeof path) != 0) {
            mem[VEG_EFFECT_RESULT] = (uint64_t)-1;
            mem[VEG_EFFECT_STATUS] = VEG_EFF_ERROR;
            return 0;
        }
        int flags = O_RDONLY;
        if (a2 == 1) flags = O_RDWR;
        if (a2 == 2) flags = O_RDWR | O_CREAT | O_TRUNC;
        int os_fd = open(path, flags, 0644);
        if (os_fd < 0) {
            mem[VEG_EFFECT_RESULT] = (uint64_t)-1;
            mem[VEG_EFFECT_STATUS] = VEG_EFF_ERROR;
            return 0;
        }
        int idx = host_fd_alloc(st, os_fd);
        if (idx < 0) {
            close(os_fd);
            mem[VEG_EFFECT_RESULT] = (uint64_t)-1;
            mem[VEG_EFFECT_STATUS] = VEG_EFF_ERROR;
            return 0;
        }
        mem[VEG_EFFECT_RESULT] = (uint64_t)idx;
        mem[VEG_EFFECT_STATUS] = VEG_EFF_DONE;
        return 0;
    }

    case VEG_EFF_CLOSE: {
        int os_fd;
        if (host_fd_get(st, a0, &os_fd) != 0) {
            mem[VEG_EFFECT_STATUS] = VEG_EFF_ERROR;
            return 0;
        }
        close(os_fd);
        host_fd_free(st, a0);
        mem[VEG_EFFECT_RESULT] = 0;
        mem[VEG_EFFECT_STATUS] = VEG_EFF_DONE;
        return 0;
    }

    case VEG_EFF_READ: {
        /* a0 = fd_idx, a1 = dst_lane, a2 = n_lanes (bytes) */
        int os_fd;
        if (host_fd_get(st, a0, &os_fd) != 0 || a1 + a2 > mem_size) {
            mem[VEG_EFFECT_RESULT] = (uint64_t)-1;
            mem[VEG_EFFECT_STATUS] = VEG_EFF_ERROR;
            return 0;
        }
        if (a2 > 4096) a2 = 4096;
        unsigned char buf[4096];
        ssize_t got = read(os_fd, buf, (size_t)a2);
        if (got < 0) {
            mem[VEG_EFFECT_RESULT] = (uint64_t)-1;
            mem[VEG_EFFECT_STATUS] = VEG_EFF_ERROR;
            return 0;
        }
        for (ssize_t i = 0; i < got; i++)
            mem[a1 + (uint64_t)i] = buf[i];
        mem[VEG_EFFECT_RESULT] = (uint64_t)got;
        mem[VEG_EFFECT_STATUS] = VEG_EFF_DONE;
        return 0;
    }

    case VEG_EFF_WRITE: {
        /* a0 = fd_idx, a1 = src_lane, a2 = n_lanes */
        int os_fd;
        if (host_fd_get(st, a0, &os_fd) != 0 || a1 + a2 > mem_size) {
            mem[VEG_EFFECT_RESULT] = (uint64_t)-1;
            mem[VEG_EFFECT_STATUS] = VEG_EFF_ERROR;
            return 0;
        }
        if (a2 > 4096) a2 = 4096;
        unsigned char buf[4096];
        for (uint64_t i = 0; i < a2; i++)
            buf[i] = (unsigned char)(mem[a1 + i] & 0xff);
        ssize_t put = write(os_fd, buf, (size_t)a2);
        if (put < 0) {
            mem[VEG_EFFECT_RESULT] = (uint64_t)-1;
            mem[VEG_EFFECT_STATUS] = VEG_EFF_ERROR;
            return 0;
        }
        mem[VEG_EFFECT_RESULT] = (uint64_t)put;
        mem[VEG_EFFECT_STATUS] = VEG_EFF_DONE;
        return 0;
    }

    default:
        mem[VEG_EFFECT_STATUS] = VEG_EFF_ERROR;
        return 0;
    }
}

#ifdef VEG_HOST_MAIN
int main(void)
{
    uint64_t mem[VEG_MEM_SIZE];
    memset(mem, 0, sizeof mem);
    veg_host_state st;
    memset(&st, 0, sizeof st);

    /* path "/tmp/veg_host_test" as lane bytes at 0x100 */
    const char *path = "/tmp/veg_host_test";
    size_t plen = strlen(path);
    for (size_t i = 0; i < plen; i++)
        mem[0x100 + i] = (unsigned char)path[i];

    veg_instr_t prog[32];
    size_t n = 0;

    /* Open create */
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_OP, VEG_EFF_OPEN, 1 } };
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_ARG0, 0x100, 1 } };
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_ARG1, plen, 1 } };
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_ARG2, 2, 1 } };
    prog[n++] = (veg_instr_t){ .op = V_EFFECT };

    /* Write 4 bytes from 0x200 */
    mem[0x200] = 'V'; mem[0x201] = 'E'; mem[0x202] = 'G'; mem[0x203] = '\n';
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_OP, VEG_EFF_WRITE, 1 } };
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_ARG0, 0, 1 } }; /* fd idx 0 */
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_ARG1, 0x200, 1 } };
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_ARG2, 4, 1 } };
    prog[n++] = (veg_instr_t){ .op = V_EFFECT };

    /* Close */
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_OP, VEG_EFF_CLOSE, 1 } };
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_ARG0, 0, 1 } };
    prog[n++] = (veg_instr_t){ .op = V_EFFECT };

    /* Exit */
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_OP, VEG_EFF_EXIT, 1 } };
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_ARG0, 0, 1 } };
    prog[n++] = (veg_instr_t){ .op = V_EFFECT };

    int rc = veg_exec_host(prog, n, mem, VEG_MEM_SIZE, veg_host_default, &st);
    printf("rc=%d exited=%d code=%d last_status=%llu last_result=%llu\n",
           rc, st.exited, st.exit_code,
           (unsigned long long)mem[VEG_EFFECT_STATUS],
           (unsigned long long)mem[VEG_EFFECT_RESULT]);
    return st.exited ? st.exit_code : 1;
}
#endif
