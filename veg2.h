#ifndef VEG2_H
#define VEG2_H

/*
 * Veggdor V2 — structured surface language
 *
 * Lowers a small C-/Lisp-flavoured syntax onto the V1 vector ISA
 * plus three control instructions (cmp / jz / jmp) and set.
 *
 * Supported statements:
 *   v8 name;
 *   v8 name = imm;
 *   v8 name = [v0, v1, ..., v7];
 *   name = name;
 *   name = name op name;          op ∈ {+, -, *}
 *   if (a OP b) stmt_or_block
 *   while (a OP b) stmt_or_block
 *   OP ∈ {==, !=, <, <=, >, >=}
 *
 * Variables occupy 8 consecutive lanes starting at an automatically
 * assigned base address. Flags for control live in a separate region.
 */

#include <stddef.h>
#include <stdint.h>
#include "veg.h"

typedef struct {
    const char *name;
    uint64_t    addr;   /* base lane address */
} veg2_var_t;

/* Parse structured source into a machine program.
 * Returns 0 on success, -1 on syntax or capacity error.
 * On success *out_n is the instruction count and *out_vars the
 * number of declared variables written into vars[0..vars_max). */
int veg2_parse(const char *text,
               veg_instr_t *prog, size_t max, size_t *out_n,
               veg2_var_t  *vars, size_t vars_max, size_t *out_vars);

#endif /* VEG2_H */
