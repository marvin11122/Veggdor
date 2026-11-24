/*
 * veg_host.c — minimal host for V_EFFECT
 *
 * Density rule: each effect is a few lanes in, a few lanes out.
 * No framework. No exception hierarchy. Host is the only place
 * real syscalls may happen.
 */

#include "veg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simple bump allocator in the high lane space (below effect region). */
#define HOST_HEAP_START  0x8000ull
#define HOST_HEAP_END    0xFD00ull

typedef struct {
    uint64_t bump;
    int      exit_code;
    int      exited;
} veg_host_state;

int veg_host_default(uint64_t *mem, size_t mem_size, void *userdata)
{
    veg_host_state *st = (veg_host_state *)userdata;
    if (!mem || VEG_EFFECT_STATUS >= mem_size) return -1;

    uint64_t op = mem[VEG_EFFECT_OP];
    uint64_t a0 = mem[VEG_EFFECT_ARG0];
    uint64_t a1 = mem[VEG_EFFECT_ARG1];
    uint64_t a2 = mem[VEG_EFFECT_ARG2];
    (void)a2;

    switch (op) {
    case VEG_EFF_NOP:
        mem[VEG_EFFECT_RESULT] = 0;
        mem[VEG_EFFECT_STATUS] = VEG_EFF_DONE;
        return 0;

    case VEG_EFF_ALLOC: {
        if (!st) { mem[VEG_EFFECT_STATUS] = VEG_EFF_ERROR; return 0; }
        if (st->bump == 0) st->bump = HOST_HEAP_START;
        uint64_t n = a0;
        if (n == 0) n = 1;
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
        /* bump allocator: free is a no-op (dense, honest) */
        mem[VEG_EFFECT_RESULT] = 0;
        mem[VEG_EFFECT_STATUS] = VEG_EFF_DONE;
        return 0;

    case VEG_EFF_EXIT:
        if (st) {
            st->exit_code = (int)a0;
            st->exited = 1;
        }
        mem[VEG_EFFECT_RESULT] = a0;
        mem[VEG_EFFECT_STATUS] = VEG_EFF_DONE;
        return 1; /* stop execution */

    case VEG_EFF_READ:
    case VEG_EFF_WRITE:
    case VEG_EFF_OPEN:
    case VEG_EFF_CLOSE:
        /* Wired for later; return error until real fd table exists */
        mem[VEG_EFFECT_RESULT] = (uint64_t)-1;
        mem[VEG_EFFECT_STATUS] = VEG_EFF_ERROR;
        return 0;

    default:
        mem[VEG_EFFECT_STATUS] = VEG_EFF_ERROR;
        return 0;
    }
}

/* Optional standalone demo: compile with -DVEG_HOST_MAIN */
#ifdef VEG_HOST_MAIN
int main(void)
{
    uint64_t mem[VEG_MEM_SIZE];
    memset(mem, 0, sizeof mem);
    veg_host_state st = {0};

    /* Program: Alloc(8), then Exit(0) */
    veg_instr_t prog[8];
    size_t n = 0;
    /* set effect op = ALLOC, arg0 = 8, status = pending */
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_OP, VEG_EFF_ALLOC, 1 } };
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_ARG0, 8, 1 } };
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_STATUS, VEG_EFF_PENDING, 1 } };
    prog[n++] = (veg_instr_t){ .op = V_EFFECT };
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_OP, VEG_EFF_EXIT, 1 } };
    prog[n++] = (veg_instr_t){ .op = V_SET, .v = { VEG_EFFECT_ARG0, 0, 1 } };
    prog[n++] = (veg_instr_t){ .op = V_EFFECT };

    int rc = veg_exec_host(prog, n, mem, VEG_MEM_SIZE, veg_host_default, &st);
    printf("rc=%d exited=%d code=%d alloc_result=%llu\n",
           rc, st.exited, st.exit_code,
           (unsigned long long)mem[VEG_EFFECT_RESULT]);
    return 0;
}
#endif
