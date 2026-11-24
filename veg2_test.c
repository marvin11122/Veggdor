#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "veg.h"
#include "veg2.h"

static uint64_t mem[VEG_MEM_SIZE];

static const char *src =
    "v8 s;\n"
    "v8 i;\n"
    "v8 n;\n"
    "v8 t;\n"
    "v8 one;\n"
    "while (i < n) {\n"
    "  t = i + one;\n"
    "  i = t;\n"
    "  s = s + i;\n"
    "}\n";

int main(void) {
    veg_instr_t prog[64];
    veg2_var_t vars[16];
    size_t np = 0, nv = 0;
    uint64_t a_s = 0, a_i = 0, a_n = 0, a_t = 0, a_one = 0;

    assert(veg2_parse(src, prog, 64, &np, vars, 16, &nv) == 0);
    assert(nv == 5);
    for (size_t k = 0; k < nv; k++) {
        if (!strcmp(vars[k].name, "s")) a_s = vars[k].addr;
        else if (!strcmp(vars[k].name, "i")) a_i = vars[k].addr;
        else if (!strcmp(vars[k].name, "n")) a_n = vars[k].addr;
        else if (!strcmp(vars[k].name, "t")) a_t = vars[k].addr;
        else if (!strcmp(vars[k].name, "one")) a_one = vars[k].addr;
    }
    assert(a_s && a_i && a_n && a_t && a_one);

    mem[a_n] = 8;   /* n = 8 */
    mem[a_one] = 1; /* one = 1 */

    assert(veg_exec(prog, np, mem, VEG_MEM_SIZE) == 0);
    assert(mem[a_s] == 36); /* sum 1..8 */

    printf("veg2_test: ok (v8 vars, while, cmp, ld/add: sum 1..8 = %llu)\n",
           (unsigned long long)mem[a_s]);

    /* V2b: constant initialisation */
    {
        static const char *src2 =
            "v8 x = 7;\n"
            "v8 y = [1, 2, 3, 4, 5, 6, 7, 8];\n";
        veg_instr_t prog2[32];
        veg2_var_t vars2[16];
        size_t np2 = 0, nv2 = 0;
        uint64_t a_x = 0, a_y = 0;
        assert(veg2_parse(src2, prog2, 32, &np2, vars2, 16, &nv2) == 0);
        assert(nv2 == 2);
        for (size_t k = 0; k < nv2; k++) {
            if (!strcmp(vars2[k].name, "x")) a_x = vars2[k].addr;
            else if (!strcmp(vars2[k].name, "y")) a_y = vars2[k].addr;
        }
        assert(veg_exec(prog2, np2, mem, VEG_MEM_SIZE) == 0);
        assert(mem[a_x] == 7);
        for (int k = 0; k < 8; k++) assert(mem[a_y + k] == (uint64_t)k + 1);
    }
    printf("veg2_test_init: ok (v8 x = 7; v8 y = [1..8])\n");
    return 0;
}
