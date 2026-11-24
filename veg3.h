#ifndef VEG3_H
#define VEG3_H

/*
 * Veggdor V3 — surface with structs and functions
 *
 * Everything still lowers to the flat lane machine (V1/V2).
 * No hidden stack, no GC, no opaque runtime.
 *
 * See V3_SPEC.md for the full contract.
 */

#include <stddef.h>
#include <stdint.h>
#include "veg.h"
#include "veg2.h"

#define VEG3_MAX_FIELDS   16
#define VEG3_MAX_PARAMS    8
#define VEG3_MAX_STRUCTS  32
#define VEG3_MAX_FUNCS    64
#define VEG3_NAME_LEN     32

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
    char       name[VEG3_NAME_LEN];
    veg3_type_t params[VEG3_MAX_PARAMS];
    uint32_t   nparams;
    veg3_type_t ret;
    uint32_t   entry_pc;   /* filled after lowering */
    uint32_t   frame_size; /* lanes needed for locals + args */
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

/* Parse a V3 compilation unit (structs + functions + top-level statements).
 * On success fills unit->prog[0..prog_len) with the fully lowered V1/V2 code.
 * Returns 0 on success, -1 on error. */
int veg3_compile(const char *src, veg3_unit_t *unit);

/* Debug: dump struct and function tables. */
void veg3_dump(const veg3_unit_t *unit, FILE *out);

#endif /* VEG3_H */
