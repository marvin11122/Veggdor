#ifndef VEG_H
#define VEG_H

/*
 * Veggdor — bare-metal vector machine
 *
 * Philosophy
 * ----------
 * Execution is understanding. Every instruction is total, deterministic,
 * bounds-checked, and fully observable. No hidden control flow in the
 * data-parallel core, no allocation inside the model, no exceptions,
 * no I/O, no probabilistic layer.
 *
 * Memory    : flat array of uint64_t lanes (two's complement).
 * Vector    : (base address, length in lanes).
 * Arithmetic: wraps mod 2^64. Aliasing is legal and intentional.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* ISA                                                                  */
/* ------------------------------------------------------------------ */

enum veg_op {
    /* V1 — pure data-parallel */
    V_LD,    /* ld  dst, src, n         dst[i] ← src[i]                     */
    V_ST,    /* st  dst, src, n         src[i] → dst[i]                     */
    V_ADD,   /* add dst, a, b, n        dst[i] ← a[i] + b[i]                */
    V_SUB,   /* sub dst, a, b, n        dst[i] ← a[i] - b[i]                */
    V_MUL,   /* mul dst, a, b, n        dst[i] ← a[i] * b[i]                */
    V_RED,   /* red dst, a, n, op       dst[0] ← reduce(a[0..n), op)        */
             /*   redop: 0=add  1=max  2=min                                 */

    /* V2 lowering — control + immediate */
    V_CMP,   /* cmp f, a, b, op         f[0] ← (a[0] op b[0]) ? 1 : 0       */
             /*   redop: 0=eq 1=ne 2=lt 3=le 4=gt 5=ge                       */
    V_SET,   /* set dst, imm, n         dst[i] ← imm   (broadcast)          */
    V_JZ,    /* jz  flag, target        if mem[flag]==0 then pc ← target    */
    V_JMP,   /* jmp target              pc ← target                         */

    V_INVALID
};

typedef struct {
    int      op;
    uint64_t v[4];   /* addresses / immediates / lengths */
    int      redop;  /* secondary opcode for RED / CMP   */
} veg_instr_t;

#define VEG_MEM_SIZE (1u << 16)   /* 65536 lanes default address space */

/* ------------------------------------------------------------------ */
/* Core API                                                             */
/* ------------------------------------------------------------------ */

/* Parse one textual instruction. Returns 0 on success, -1 on error. */
int  veg_parse_line(const char *line, veg_instr_t *out);

/* Parse a whole file of V1 instructions. Skips blank/comment lines. */
int  veg_parse_file(FILE *f, veg_instr_t *prog, size_t max, size_t *out_n);

/* Execute the entire program. Bounds-checked. Returns 0 or -1. */
int  veg_exec(const veg_instr_t *prog, size_t n,
              uint64_t *mem, size_t mem_size);

/* ------------------------------------------------------------------ */
/* Observability — the part that makes the machine inspectable          */
/* ------------------------------------------------------------------ */

/* Disassemble one instruction into buf. Always null-terminates.
 * Returns bytes that would have been written (like snprintf). */
int  veg_disasm(const veg_instr_t *in, char *buf, size_t buflen);

/* Single-step. *pc is the current program counter (updated in place).
 * Returns 0 on success, -1 on fault, 1 if program ended (pc >= n). */
int  veg_step(const veg_instr_t *prog, size_t n,
              uint64_t *mem, size_t mem_size, size_t *pc);

/* Execute with optional trace callback.
 * trace(pc, instr, mem, mem_size, userdata) is called *before* each insn.
 * If trace returns non-zero, execution stops early (still returns 0). */
typedef int (*veg_trace_fn)(size_t pc, const veg_instr_t *in,
                            const uint64_t *mem, size_t mem_size,
                            void *userdata);

int  veg_exec_trace(const veg_instr_t *prog, size_t n,
                    uint64_t *mem, size_t mem_size,
                    veg_trace_fn trace, void *userdata);

#endif /* VEG_H */
