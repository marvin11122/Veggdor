#ifndef VEG_H
#define VEG_H

/*
 * Veggdor V1/V2 — bare-metal vector machine
 *
 * Memory model: flat array of 64-bit two's-complement lanes.
 * A vector is the pair (base address, length in lanes).
 * All arithmetic wraps mod 2^64. Aliasing is permitted.
 *
 * Design goal: every instruction is directly executable and fully
 * observable. No hidden control flow in the core, no allocation,
 * no exceptions, no I/O inside the execution model.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum veg_op {
    /* Core data-parallel ops (V1) */
    V_LD,   /* ld  dst, src, n          — dst[i] ← src[i]                    */
    V_ST,   /* st  dst, src, n          — src[i] → dst[i]                    */
    V_ADD,  /* add dst, a, b, n         — dst[i] ← a[i] + b[i]               */
    V_SUB,  /* sub dst, a, b, n         — dst[i] ← a[i] - b[i]               */
    V_MUL,  /* mul dst, a, b, n         — dst[i] ← a[i] * b[i]               */
    V_RED,  /* red dst, a, n, op        — dst[0] ← reduce(a[0..n), op)       */
            /*   redop: 0=add, 1=max, 2=min                                  */

    /* Control + immediate (V2 lowering) */
    V_CMP,  /* cmp f, a, b, op          — f[0] ← (a[0] op b[0]) ? 1 : 0      */
            /*   redop: 0=eq 1=ne 2=lt 3=le 4=gt 5=ge                         */
    V_SET,  /* set dst, imm, n          — dst[i] ← imm  (broadcast)          */
    V_JZ,   /* jz  flag, target_pc      — if mem[flag]==0 then pc ← target   */
    V_JMP,  /* jmp target_pc            — pc ← target                        */

    V_INVALID
};

typedef struct {
    int      op;     /* veg_op */
    uint64_t v[4];   /* operand fields (addresses / immediates / lengths) */
    int      redop;  /* secondary opcode for RED and CMP */
} veg_instr_t;

#define VEG_MEM_SIZE (1u << 16)  /* 65536 lanes — default address space */

/* Parse one textual instruction ("add 0x20, 0x8, 0x10, 8").
 * Returns 0 on success, -1 on syntax error. */
int veg_parse_line(const char *line, veg_instr_t *out);

/* Execute n instructions against memory. Bounds-checked.
 * Returns 0 on success, -1 on out-of-bounds or invalid op. */
int veg_exec(const veg_instr_t *prog, size_t n,
             uint64_t *mem, size_t mem_size);

/* Parse an entire file of V1 instructions into prog[0..max).
 * Skips blank/comment lines. Returns 0 and sets *out_n on success. */
int veg_parse_file(FILE *f, veg_instr_t *prog, size_t max, size_t *out_n);

#endif /* VEG_H */
