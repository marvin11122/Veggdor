/*
 * veg_mc — standalone machine-code runner for the Veggdor V1 ISA.
 *
 * Reads a text program of machine instructions (veg_parse_line),
 * prints its disassembly, executes it with veg_exec, and dumps
 * the resulting nonzero memory lanes. Uses ONLY the V1 ISA.
 */
#include "veg.h"

#include <stdio.h>

#define VEG_MC_MAX_PROG 2048

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <program.mc>\n", argv[0]);
        return 2;
    }

    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror("veg_mc: fopen");
        return 1;
    }

    static veg_instr_t prog[VEG_MC_MAX_PROG];
    size_t n = 0;
    if (veg_parse_file(f, prog, VEG_MC_MAX_PROG, &n) != 0 || n == 0) {
        fprintf(stderr, "veg_mc: parse error in %s\n", argv[1]);
        fclose(f);
        return 1;
    }
    fclose(f);

    char buf[128];
    printf("program: %zu instructions\n", n);
    for (size_t i = 0; i < n; i++) {
        veg_disasm(&prog[i], buf, sizeof buf);
        printf("  %04zu  %s\n", i, buf);
    }

    static uint64_t mem[VEG_MEM_SIZE];
    if (veg_exec(prog, n, mem, VEG_MEM_SIZE) != 0) {
        fprintf(stderr, "veg_mc: exec error\n");
        return 1;
    }

    printf("memory (nonzero lanes):\n");
    for (size_t i = 0; i < VEG_MEM_SIZE; i++)
        if (mem[i] != 0)
            printf("  0x%04zx  0x%llx\n", i, (unsigned long long)mem[i]);
    return 0;
}
