#ifndef VEG_H
#define VEG_H

/*
 * Veggdor — bare-metal vector machine
 *
 * Execution is understanding. Every instruction is total, deterministic,
 * bounds-checked, and fully observable.
 *
 * Capability (I/O, alloc, …) enters only through V_EFFECT + host callback.
 * No silent syscalls. No ambient authority.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum veg_op {
    V_LD,
    V_ST,
    V_ADD,
    V_SUB,
    V_MUL,
    V_RED,      /* redop: 0=add 1=max 2=min */
    V_CMP,      /* redop: 0=eq 1=ne 2=lt 3=le 4=gt 5=ge */
    V_SET,
    V_JZ,
    V_JMP,
    V_EFFECT,   /* trap to host; descriptor at VEG_EFFECT_BASE */
    V_INVALID
};

/* Effect descriptor lanes (see EFFECTS.md) */
#define VEG_EFFECT_BASE    0xFE00ull
#define VEG_EFFECT_OP      (VEG_EFFECT_BASE + 0)
#define VEG_EFFECT_ARG0    (VEG_EFFECT_BASE + 1)
#define VEG_EFFECT_ARG1    (VEG_EFFECT_BASE + 2)
#define VEG_EFFECT_ARG2    (VEG_EFFECT_BASE + 3)
#define VEG_EFFECT_RESULT  (VEG_EFFECT_BASE + 4)
#define VEG_EFFECT_STATUS  (VEG_EFFECT_BASE + 5)

enum veg_effect_op {
    VEG_EFF_NOP   = 0,
    VEG_EFF_ALLOC = 1,
    VEG_EFF_FREE  = 2,
    VEG_EFF_READ  = 3,
    VEG_EFF_WRITE = 4,
    VEG_EFF_OPEN  = 5,
    VEG_EFF_CLOSE = 6,
    VEG_EFF_EXIT  = 7
};

enum veg_effect_status {
    VEG_EFF_PENDING = 0,
    VEG_EFF_DONE    = 1,
    VEG_EFF_ERROR   = 2
};

typedef struct {
    int      op;
    uint64_t v[4];
    int      redop;
} veg_instr_t;

#define VEG_MEM_SIZE (1u << 16)

/* Host callback: called when V_EFFECT executes.
 * Must read mem[VEG_EFFECT_*], perform the op, write RESULT+STATUS.
 * Return 0 to continue, non-zero to abort execution. */
typedef int (*veg_host_fn)(uint64_t *mem, size_t mem_size, void *userdata);

int  veg_parse_line(const char *line, veg_instr_t *out);
int  veg_parse_file(FILE *f, veg_instr_t *prog, size_t max, size_t *out_n);
int  veg_exec(const veg_instr_t *prog, size_t n,
              uint64_t *mem, size_t mem_size);
int  veg_exec_host(const veg_instr_t *prog, size_t n,
                   uint64_t *mem, size_t mem_size,
                   veg_host_fn host, void *userdata);
int  veg_disasm(const veg_instr_t *in, char *buf, size_t buflen);
int  veg_step(const veg_instr_t *prog, size_t n,
              uint64_t *mem, size_t mem_size, size_t *pc);
int  veg_step_host(const veg_instr_t *prog, size_t n,
                   uint64_t *mem, size_t mem_size, size_t *pc,
                   veg_host_fn host, void *userdata);

typedef int (*veg_trace_fn)(size_t pc, const veg_instr_t *in,
                            const uint64_t *mem, size_t mem_size,
                            void *userdata);

int  veg_exec_trace(const veg_instr_t *prog, size_t n,
                    uint64_t *mem, size_t mem_size,
                    veg_trace_fn trace, void *userdata);

#endif /* VEG_H */
