#include <stdio.h>
#include <string.h>
#include "veg.h"
#include "veg2.h"

static uint64_t mem[VEG_MEM_SIZE];

static uint64_t var_of(veg2_var_t *vars, size_t nv, const char *name) {
    for (size_t i = 0; i < nv; i++)
        if (!strcmp(vars[i].name, name)) return vars[i].addr;
    return 0;
}

int main(void) {
    static veg_instr_t prog[64];
    static veg2_var_t vars[16];
    size_t np = 0, nv = 0;
    uint64_t a_s, a_i, a_n, a_one;

    FILE *f = fopen("example.v", "r");
    if (!f) {
        perror("example.v");
        return 1;
    }
    static char buf[4096];
    size_t len = fread(buf, 1, sizeof buf - 1, f);
    buf[len] = 0;
    fclose(f);

    if (veg2_parse(buf, prog, 64, &np, vars, 16, &nv) != 0) {
        printf("veg2_run: parse error\n");
        return 1;
    }
    a_s = var_of(vars, nv, "s");
    a_i = var_of(vars, nv, "i");
    a_n = var_of(vars, nv, "n");
    a_one = var_of(vars, nv, "one");

    mem[a_n] = 8;
    mem[a_one] = 1;

    if (veg_exec(prog, np, mem, VEG_MEM_SIZE) != 0) {
        printf("veg2_run: exec error\n");
        return 1;
    }
    printf("veg2_run: s=%llu i=%llu n=%llu\n",
           (unsigned long long)mem[a_s],
           (unsigned long long)mem[a_i],
           (unsigned long long)mem[a_n]);
    return 0;
}
