#include <assert.h>
#include <stdio.h>
#include "veg.h"

static uint64_t mem[VEG_MEM_SIZE];

int main(void) {
    static veg_instr_t prog[8];
    size_t np;

    for (uint64_t i = 0; i < VEG_MEM_SIZE; i++) mem[i] = i + 1;

    /* add: a=0x0, b=0x8, out=0x20 -> out[k] = a[k]+b[k] = (k+1)+(k+9) */
    assert(veg_parse_line("add 0x20, 0x0, 0x8, 8", &prog[0]) == 0);
    np = 1;
    assert(veg_exec(prog, np, mem, VEG_MEM_SIZE) == 0);
    for (int k = 0; k < 8; k++)
        assert(mem[0x20 + k] == (uint64_t)(k + 1) + (uint64_t)(k + 9));

    /* mul: out[k] = a[k]*b[k] */
    assert(veg_parse_line("mul 0x28, 0x0, 0x8, 8", &prog[0]) == 0);
    np = 1;
    assert(veg_exec(prog, np, mem, VEG_MEM_SIZE) == 0);
    for (int k = 0; k < 8; k++)
        assert(mem[0x28 + k] == (uint64_t)(k + 1) * (uint64_t)(k + 9));

    /* red add over a=0x0 (lanes 1..8): sum = 36 */
    assert(veg_parse_line("red 0x30, 0x0, 8, add", &prog[0]) == 0);
    np = 1;
    assert(veg_exec(prog, np, mem, VEG_MEM_SIZE) == 0);
    assert(mem[0x30] == 36);

    /* red max over b=0x8 (lanes 9..16): max = 16 */
    assert(veg_parse_line("red 0x38, 0x8, 8, max", &prog[0]) == 0);
    np = 1;
    assert(veg_exec(prog, np, mem, VEG_MEM_SIZE) == 0);
    assert(mem[0x38] == 16);

    /* red min over a=0x0 (lanes 1..8): min = 1 */
    assert(veg_parse_line("red 0x40, 0x0, 8, min", &prog[0]) == 0);
    np = 1;
    assert(veg_exec(prog, np, mem, VEG_MEM_SIZE) == 0);
    assert(mem[0x40] == 1);

    /* ld/st: mem[0x48+k] = mem[0x8+k], k < 4 */
    assert(veg_parse_line("ld 0x48, 0x8, 4", &prog[0]) == 0);
    np = 1;
    assert(veg_exec(prog, np, mem, VEG_MEM_SIZE) == 0);
    for (int k = 0; k < 4; k++) assert(mem[0x48 + k] == mem[0x8 + k]);

    printf("veg_test: ok (add/mul/red-add/red-max/red-min/ld/st)\n");
    return 0;
}
