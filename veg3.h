#ifndef VEG3_H
#define VEG3_H

/*
 * Veggdor V3 — surface with structs and functions
 *
 * Everything still lowers to the flat lane machine (V1/V2).
 * No hidden stack, no GC, no opaque runtime.
 *
 * Call convention (total, visible):
 *   0xF000  : continuation PC (return address)
 *   0xF100  : argument region (caller writes, callee reads)
 *   0xF200  : return-value region (callee writes, caller reads)
 *
 * See V3_SPEC.md for the full contract.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "veg.h"
#include "veg2.h"

#define VEG3_MAX_FIELDS   16
#define VEG3_MAX_PARAMS    8
#define VEG3_MAX_STRUCTS  32
#define VEG3_MAX_FUNCS    64
#define VEG3_NAME_LEN     32

/* Fixed lane regions for the call convention */
#define VEG3_RET_PC_ADDR   0xF000ull
#define VEG3_ARG_BASE      0xF100ull
#define VEG3_RETVAL_BASE   0xF200ull

typedef struct {
    char     name[VEG3_NAME_LEN];
    uint32_t offset;   /* lane offset from struct base */
    uint32_t size;     /* in lanes */
} veg3_field_t;

typedef struct {
    char         name[VEG3_NAME_LEN];
    veg3_field_t fields[VEG3_MAX_FIELDS];
    uint32_t     nfields;
    uint32_t     size;     /* total lanes */
} veg3_struct_t;

typedef struct {
    char     name[VEG3_NAME_LEN];
    uint32_t size;         /* in lanes (1 for v1, 8 for v8, or struct size) */
    int      is_struct;    /* 1 if type is a named struct */
    int      struct_idx;   /* index into struct table if is_struct */
} veg3_type_t;

typedef struct {
    char        name[VEG3_NAME_LEN];
    veg3_type_t params[VEG3_MAX_PARAMS];
    char        param_names[VEG3_MAX_PARAMS][VEG3_NAME_LEN];
    uint32_t    nparams;
    veg3_type_t ret;
    uint32_t    entry_pc;    /* filled after lowering */
    uint32_t    frame_size;  /* lanes needed for locals + args */
    int         has_ret;     /* 1 if function has a return type */
} veg3_func_t;

typedef struct {
    veg3_struct_t structs[VEG3_MAX_STRUCTS];
    uint32_t      nstructs;
    veg3_func_t   funcs[VEG3_MAX_FUNCS];
    uint32_t      nfuncs;

    /* lowered output */
    veg_instr_t  *prog;
    size_t        prog_cap;
    size_t        prog_len;

    /* simple symbol table for variables (name → base lane) */
    veg2_var_t   *vars;
    size_t        vars_cap;
    size_t        vars_len;
    uint64_t      next_addr;
} veg3_unit_t;

/* Parse a V3 compilation unit (structs + functions).
 * On success fills unit->prog[0..prog_len) with the fully lowered V1/V2 code.
 * Caller must provide unit->prog / unit->prog_cap (and optionally vars).
 * Returns 0 on success, -1 on error. */
int veg3_compile(const char *src, veg3_unit_t *unit);

/* Emit a call sequence:
 *   copy args → ARG_BASE, set RET_PC, jmp entry.
 * arg_bases[i] / arg_sizes[i] describe each argument in caller memory.
 * Returns 0 on success. */
int veg3_emit_call(veg3_unit_t *u, uint32_t func_idx,
                   const uint64_t *arg_bases, const uint32_t *arg_sizes,
                   uint32_t nargs);

/* Emit a return sequence:
 *   copy retval → RETVAL_BASE (if size>0), jmp *RET_PC.
 * Returns 0 on success. */
int veg3_emit_ret(veg3_unit_t *u, uint64_t retval_base, uint32_t retval_size);

/* Debug: dump struct and function tables + instruction count. */
void veg3_dump(const veg3_unit_t *unit, FILE *out);

#endif /* VEG3_H */
