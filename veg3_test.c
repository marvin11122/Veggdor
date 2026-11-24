#include "veg3.h"

#include <stdio.h>
#include <string.h>

static int fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int find_func3(veg3_unit_t *u, const char *name)
{
    for (uint32_t i = 0; i < u->nfuncs; i++)
        if (strcmp(u->funcs[i].name, name) == 0) return (int)i;
    return -1;
}

int main(void)
{
    static veg_instr_t prog[512];
    veg3_unit_t unit;
    memset(&unit, 0, sizeof unit);
    unit.prog = prog;
    unit.prog_cap = 512;

    const char *src =
        "struct Point {\n"
        "  x: v1;\n"
        "  y: v1;\n"
        "}\n"
        "\n"
        "fn add_v1(a: v1, b: v1) -> v1 {\n"
        "  v1 r;\n"
        "  r = a + b;\n"
        "  return r;\n"
        "}\n"
        "\n"
        "fn id(x: v1) -> v1 {\n"
        "  return x;\n"
        "}\n";

    if (veg3_compile(src, &unit) != 0)
        return fail("compile");

    if (unit.nstructs != 1)
        return fail("expected 1 struct");
    if (unit.nfuncs != 2)
        return fail("expected 2 functions");
    if (unit.prog_len == 0)
        return fail("no instructions emitted");

    printf("V3 smoke test OK\n");
    veg3_dump(&unit, stdout);

    /* Execute id(42): set up arg, call, check retval */
    uint64_t mem[VEG_MEM_SIZE];
    memset(mem, 0, sizeof mem);

    /* Find id */
    int id_idx = -1;
    for (uint32_t i = 0; i < unit.nfuncs; i++)
        if (strcmp(unit.funcs[i].name, "id") == 0) id_idx = (int)i;
    if (id_idx < 0) return fail("id not found");

    /* Manual call sequence at end of prog for the test harness */
    size_t start = unit.prog_len;
    mem[0x400] = 42; /* argument value in a test lane */
    uint64_t abases[1] = { 0x400 };
    uint32_t asizes[1] = { 1 };
    if (veg3_emit_call(&unit, (uint32_t)id_idx, abases, asizes, 1) != 0)
        return fail("emit_call");

    /* Run from the harness call site */
    size_t pc = start;
    while (pc < unit.prog_len) {
        int r = veg_step(unit.prog, unit.prog_len, mem, VEG_MEM_SIZE, &pc);
        if (r < 0) return fail("exec step");
        if (r > 0) break;
        /* safety */
        if (pc > unit.prog_len + 10) return fail("runaway");
    }

    if (mem[VEG3_RETVAL_BASE] != 42) {
        fprintf(stderr, "retval=%llu expected 42\n",
                (unsigned long long)mem[VEG3_RETVAL_BASE]);
        return fail("retval");
    }

    printf("id(42) -> %llu  OK\n", (unsigned long long)mem[VEG3_RETVAL_BASE]);

    /* V3b: nested struct fields — Outer { p: Inner; q: v1; } */
    {
        static veg_instr_t prog3[512];
        static veg3_unit_t unit3;
        memset(&unit3, 0, sizeof unit3);
        unit3.prog = prog3;
        unit3.prog_cap = 512;

        const char *src3 =
            "struct Inner { a: v1; b: v1; }\n"
            "struct Outer { p: Inner; q: v1; }\n"
            "fn read_a() -> v1 {\n"
            "  Outer o;\n"
            "  o.p.a = 41;\n"
            "  return o.p.a;\n"
            "}\n";

        if (veg3_compile(src3, &unit3) != 0)
            return fail("compile3");
        if (unit3.nstructs != 2)
            return fail("expected 2 structs (nested)");
        if (unit3.nfuncs != 1)
            return fail("expected 1 fn (read_a)");

        int ra = find_func3(&unit3, "read_a");
        if (ra < 0) return fail("read_a not found");

        uint64_t mem3[VEG_MEM_SIZE];
        memset(mem3, 0, sizeof mem3);
        size_t start = unit3.prog_len;
        if (veg3_emit_call(&unit3, (uint32_t)ra, NULL, NULL, 0) != 0)
            return fail("emit_call3");

        size_t pc = start;
        while (pc < unit3.prog_len) {
            int r = veg_step(unit3.prog, unit3.prog_len,
                             mem3, VEG_MEM_SIZE, &pc);
            if (r < 0) return fail("exec3 step");
            if (r > 0) break;
            if (pc > unit3.prog_len + 10) return fail("runaway3");
        }
        if (mem3[VEG3_RETVAL_BASE] != 41) {
            fprintf(stderr, "retval3=%llu expected 41\n",
                    (unsigned long long)mem3[VEG3_RETVAL_BASE]);
            return fail("retval3");
        }
        printf("read_a() -> %llu (nested struct field)  OK\n",
               (unsigned long long)mem3[VEG3_RETVAL_BASE]);
    }
    printf("veg3_test OK\n");
    return 0;
}
