#ifndef VEG2_H
#define VEG2_H

#include <stddef.h>
#include <stdint.h>
#include "veg.h"

typedef struct {
    const char *name;
    uint64_t addr;
} veg2_var_t;

/* Parse the structured "new C" language (V2) into the machine program.
   Statements:  v8 name;
                name = name;
                name = name op name;   (op in {+, -, *})
                if (a OP b) stmt;
                while (a OP b) stmt;
   OP in {<, <=, >, >=, ==, !=}.  Returns 0 on success. */
int veg2_parse(const char *text, veg_instr_t *prog, size_t max, size_t *out_n,
               veg2_var_t *vars, size_t vars_max, size_t *out_vars);

#endif
