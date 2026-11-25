/*
 * veg_mc — standalone machine-code runner for the Veggdor V1 ISA.
 *
 * Reads a text program of machine instructions (veg_parse_line),
 * prints its disassembly, executes it with veg_exec, and dumps
 * the resulting nonzero memory lanes. Uses ONLY the V1 ISA.
 */
#include "veg.h"

#include <stdio.h>
#include <string.h>

#define VEG_MC_MAX_PROG 2048
#define VEG_MC_LINE_MAX 512

typedef struct {
    uint64_t previous[VEG_MEM_SIZE];
    size_t step;
} trace_state_t;

static int trace_step(size_t pc, const veg_instr_t *in,
                      const uint64_t *mem, size_t mem_size, void *userdata)
{
    trace_state_t *state = userdata;
    char disasm[128];
    veg_disasm(in, disasm, sizeof disasm);
    int changed = 0;
    for (size_t i = 0; i < mem_size; i++) {
        if (state->previous[i] != mem[i]) {
            if (!changed)
                printf("state changes since previous step:\n");
            changed = 1;
            printf("  mem[0x%04zx]: 0x%llx -> 0x%llx\n", i,
                   (unsigned long long)state->previous[i],
                   (unsigned long long)mem[i]);
        }
    }
    printf("step %zu pc=%zu  %s\n", state->step++, pc, disasm);
    memcpy(state->previous, mem, mem_size * sizeof *mem);
    return 0;
}

int main(int argc, char **argv)
{
    int trace = 0;
    const char *path = NULL;
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 ||
                      strcmp(argv[1], "-h") == 0)) {
        printf("usage: %s [--trace] <program.mc>\n"
               "       %s --help\n"
               "Executes Veggdor V1 machine code in zeroed memory.\n"
               "--trace prints each instruction and lane changes.\n",
               argv[0], argv[0]);
        return 0;
    }
    if (argc == 2) {
        path = argv[1];
    } else if (argc == 3 && strcmp(argv[1], "--trace") == 0) {
        trace = 1;
        path = argv[2];
    } else {
        fprintf(argc == 2 ? stdout : stderr,
                "usage: %s [--trace] <program.mc>\n", argv[0]);
        return 2;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        perror("veg_mc: fopen");
        return 1;
    }

    static veg_instr_t prog[VEG_MC_MAX_PROG];
    size_t n = 0;
    char line[VEG_MC_LINE_MAX];
    size_t line_no = 0;
    int parse_failed = 0;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] != '\n' && !feof(f)) {
            fprintf(stderr, "veg_mc: %s:%zu: line exceeds %d bytes\n",
                    path, line_no, VEG_MC_LINE_MAX - 1);
            parse_failed = 1;
            break;
        }
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';
        char *p = line;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p == '\0' || *p == ';') continue;
        if (n == VEG_MC_MAX_PROG || veg_parse_line(p, &prog[n]) != 0) {
            fprintf(stderr, "veg_mc: %s:%zu: invalid instruction\n",
                    path, line_no);
            parse_failed = 1;
            break;
        }
        n++;
    }
    if (ferror(f)) {
        perror("veg_mc: read");
        parse_failed = 1;
    }
    fclose(f);
    if (parse_failed) return 1;
    if (n == 0) {
        fprintf(stderr, "veg_mc: %s: no instructions\n", path);
        return 1;
    }

    char buf[128];
    printf("program: %zu instructions\n", n);
    if (!trace) {
        for (size_t i = 0; i < n; i++) {
            veg_disasm(&prog[i], buf, sizeof buf);
            printf("  %04zu  %s\n", i, buf);
        }
    }

    static uint64_t mem[VEG_MEM_SIZE];
    trace_state_t trace_state = { 0 };
    int exec_result = trace
        ? veg_exec_trace(prog, n, mem, VEG_MEM_SIZE, trace_step, &trace_state)
        : veg_exec(prog, n, mem, VEG_MEM_SIZE);
    if (exec_result != 0) {
        fprintf(stderr, "veg_mc: exec error\n");
        return 1;
    }

    printf("memory (nonzero lanes):\n");
    for (size_t i = 0; i < VEG_MEM_SIZE; i++)
        if (mem[i] != 0)
            printf("  0x%04zx  0x%llx\n", i, (unsigned long long)mem[i]);
    return 0;
}
