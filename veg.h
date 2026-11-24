#ifndef VEG_H
#define VEG_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum veg_op {
    V_LD,  /* ld  dst, src, n                                                    */
    V_ST,  /* st  dst, src, n                                                    */
    V_ADD, /* add dst, a, b, n                                                   */
    V_SUB, /* sub dst, a, b, n                                                   */
    V_MUL, /* mul dst, a, b, n                                                   */
    V_RED, /* red dst, a, n, op (0 add, 1 max, 2 min)                            */
    V_CMP, /* cmp f, a, b, redop (0 eq, 1 ne, 2 lt, 3 le, 4 gt, 5 ge): f[0] = ... */
    V_SET, /* set dst, imm, n:                                                   */
    V_JZ,  /* jz  flag, target_pc: jump if mem[flag] == 0                        */
    V_JMP, /* jmp target_pc                                                      */
    V_INVALID
};

typedef struct {
    int op;
    uint64_t v[4];
    int redop;
} veg_instr_t;

#define VEG_MEM_SIZE (1u << 16)

/* Parse one instruction line ("add 0x20, 0x8, 0x10, 8"). Returns 0 on success. */
int veg_parse_line(const char *line, veg_instr_t *in);

/* Execute n instructions against memory (bounds-checked). Returns 0 on success, -1 on violation. */
int veg_exec(const veg_instr_t *prog, size_t n, uint64_t *mem, size_t mem_size);

/* Parse a whole file into prog. Returns 0 and sets *out_n on success, -1 on parse error. */
int veg_parse_file(FILE *f, veg_instr_t *prog, size_t max, size_t *out_n);

#endif
