#include "veg.h"

#include <stdlib.h>
#include <string.h>

static int op_from_name(const char *s, int *op) {
    if (strlen(s) == 2 && memcmp(s, "ld", 2) == 0) { *op = V_LD; return 0; }
    if (strlen(s) == 2 && memcmp(s, "st", 2) == 0) { *op = V_ST; return 0; }
    if (strlen(s) == 3 && memcmp(s, "add", 3) == 0) { *op = V_ADD; return 0; }
    if (strlen(s) == 3 && memcmp(s, "sub", 3) == 0) { *op = V_SUB; return 0; }
    if (strlen(s) == 3 && memcmp(s, "mul", 3) == 0) { *op = V_MUL; return 0; }
    if (strlen(s) == 3 && memcmp(s, "red", 3) == 0) { *op = V_RED; return 0; }
    return -1;
}

static int parse_u64(const char *s, uint64_t *out) {
    char *end = NULL;
    unsigned long long v = strtoull(s, &end, 0);
    if (end == s || *end != '\0') return -1;
    *out = (uint64_t)v;
    return 0;
}

int veg_parse_line(const char *line, veg_instr_t *in) {
    char buf[512];
    char *tok[8];
    int ntok = 0, op = V_INVALID;

    if (snprintf(buf, sizeof buf, "%s", line) >= (int)sizeof buf) return -1;
    char *semi = strchr(buf, ';');
    if (semi) *semi = '\0';

    for (char *t = strtok(buf, " \t\r\n,"); t != NULL && ntok < 8;
         t = strtok(NULL, " \t\r\n,"))
        tok[ntok++] = t;
    if (ntok == 0) return -1;
    if (op_from_name(tok[0], &op) != 0) return -1;

    switch (op) {
    case V_LD:
    case V_ST:
        if (ntok != 4) return -1;
        in->v[0] = in->v[1] = in->v[2] = 0;
        in->redop = 0;
        if (parse_u64(tok[1], &in->v[0]) || parse_u64(tok[2], &in->v[1]) ||
            parse_u64(tok[3], &in->v[2])) return -1;
        break;
    case V_ADD:
    case V_SUB:
    case V_MUL:
        if (ntok != 5) return -1;
        in->redop = 0;
        if (parse_u64(tok[1], &in->v[0]) || parse_u64(tok[2], &in->v[1]) ||
            parse_u64(tok[3], &in->v[2]) || parse_u64(tok[4], &in->v[3]))
            return -1;
        break;
    case V_RED:
        if (ntok != 5) return -1;
        if (parse_u64(tok[1], &in->v[0]) || parse_u64(tok[2], &in->v[1]) ||
            parse_u64(tok[3], &in->v[2])) return -1;
        if (strcmp(tok[4], "add") == 0) in->redop = 0;
        else if (strcmp(tok[4], "max") == 0) in->redop = 1;
        else if (strcmp(tok[4], "min") == 0) in->redop = 2;
        else {
            long v = strtol(tok[4], NULL, 0);
            if (v < 0 || v > 2) return -1;
            in->redop = (int)v;
        }
        break;
    default:
        return -1;
    }
    in->op = op;
    return 0;
}

int veg_exec(const veg_instr_t *prog, size_t n, uint64_t *mem, size_t mem_size) {
    size_t pc = 0;
    while (pc < n) {
        const veg_instr_t *in = &prog[pc++];
        switch (in->op) {
        case V_SET: {
            uint64_t dst = in->v[0], imm = in->v[1], len = in->v[2];
            if (dst + len > mem_size) return -1;
            for (uint64_t k = 0; k < len; k++) mem[dst + k] = imm;
            break;
        }
        case V_JMP:
            if (in->v[0] >= n) return -1;
            pc = in->v[0];
            continue;
        case V_JZ:
            if (in->v[0] >= mem_size || in->v[1] > n) return -1;
            if (mem[in->v[0]] == 0) pc = in->v[1];
            continue;
        case V_CMP: {
            uint64_t f = in->v[0], a = in->v[1], b = in->v[2];
            if (f >= mem_size || a + 1 > mem_size || b + 1 > mem_size)
                return -1;
            uint64_t x = mem[a], y = mem[b];
            int r = 0;
            switch (in->redop) {
            case 0: r = (x == y); break;
            case 1: r = (x != y); break;
            case 2: r = (x < y); break;
            case 3: r = (x <= y); break;
            case 4: r = (x > y); break;
            case 5: r = (x >= y); break;
            default: return -1;
            }
            mem[f] = r ? 1 : 0;
            break;
        }
        case V_LD:
        case V_ST: {
            uint64_t dst = in->v[0], src = in->v[1], len = in->v[2];
            if (dst + len > mem_size || src + len > mem_size) return -1;
            for (uint64_t k = 0; k < len; k++) mem[dst + k] = mem[src + k];
            break;
        }
        case V_ADD:
        case V_SUB:
        case V_MUL: {
            uint64_t dst = in->v[0], a = in->v[1], b = in->v[2], len = in->v[3];
            if (dst + len > mem_size || a + len > mem_size || b + len > mem_size)
                return -1;
            for (uint64_t k = 0; k < len; k++) {
                uint64_t x = mem[a + k], y = mem[b + k];
                mem[dst + k] =
                    in->op == V_ADD ? x + y : in->op == V_SUB ? x - y : x * y;
            }
            break;
        }
        case V_RED: {
            uint64_t dst = in->v[0], a = in->v[1], len = in->v[2];
            if (dst >= mem_size || a + len > mem_size || len == 0) return -1;
            uint64_t acc = mem[a];
            for (uint64_t k = 1; k < len; k++) {
                uint64_t v = mem[a + k];
                if (in->redop == 1 && v > acc) acc = v;
                else if (in->redop == 2 && v < acc) acc = v;
                else if (in->redop == 0) acc += v;
            }
            mem[dst] = acc;
            break;
        }
        default:
            return -1;
        }
    }
    return 0;
}

int veg_parse_file(FILE *f, veg_instr_t *prog, size_t max, size_t *out_n) {
    char line[512];
    size_t n = 0;
    *out_n = 0;
    while (fgets(line, sizeof line, f)) {
        veg_instr_t in;
        if (veg_parse_line(line, &in) != 0) continue;
        if (n >= max) return -1;
        prog[n++] = in;
    }
    *out_n = n;
    return 0;
}
