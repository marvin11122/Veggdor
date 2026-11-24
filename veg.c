#include "veg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int op_from_name(const char *s, int *op)
{
    if (!s) return -1;
    if (strcmp(s, "ld")  == 0) { *op = V_LD;  return 0; }
    if (strcmp(s, "st")  == 0) { *op = V_ST;  return 0; }
    if (strcmp(s, "add") == 0) { *op = V_ADD; return 0; }
    if (strcmp(s, "sub") == 0) { *op = V_SUB; return 0; }
    if (strcmp(s, "mul") == 0) { *op = V_MUL; return 0; }
    if (strcmp(s, "red") == 0) { *op = V_RED; return 0; }
    if (strcmp(s, "cmp") == 0) { *op = V_CMP; return 0; }
    if (strcmp(s, "set") == 0) { *op = V_SET; return 0; }
    if (strcmp(s, "jz")  == 0) { *op = V_JZ;  return 0; }
    if (strcmp(s, "jmp") == 0) { *op = V_JMP; return 0; }
    if (strcmp(s, "effect") == 0) { *op = V_EFFECT; return 0; }
    return -1;
}

static int parse_u64(const char *s, uint64_t *out)
{
    char *end = NULL;
    if (!s || !*s) return -1;
    unsigned long long v = strtoull(s, &end, 0);
    if (end == s || *end != '\0') return -1;
    *out = (uint64_t)v;
    return 0;
}

int veg_parse_line(const char *line, veg_instr_t *in)
{
    char buf[512];
    char *tok[8];
    int ntok = 0, op = V_INVALID;
    if (!line || !in) return -1;
    if (snprintf(buf, sizeof buf, "%s", line) >= (int)sizeof buf) return -1;
    char *semi = strchr(buf, ';');
    if (semi) *semi = '\0';
    for (char *t = strtok(buf, " \t\r\n,"); t && ntok < 8; t = strtok(NULL, " \t\r\n,"))
        tok[ntok++] = t;
    if (ntok == 0 || op_from_name(tok[0], &op) != 0) return -1;
    memset(in, 0, sizeof *in);
    in->op = op;

    switch (op) {
    case V_EFFECT:
        if (ntok != 1) return -1;
        break;
    case V_LD: case V_ST:
        if (ntok != 4) return -1;
        if (parse_u64(tok[1], &in->v[0]) || parse_u64(tok[2], &in->v[1]) ||
            parse_u64(tok[3], &in->v[2])) return -1;
        break;
    case V_ADD: case V_SUB: case V_MUL:
        if (ntok != 5) return -1;
        if (parse_u64(tok[1], &in->v[0]) || parse_u64(tok[2], &in->v[1]) ||
            parse_u64(tok[3], &in->v[2]) || parse_u64(tok[4], &in->v[3])) return -1;
        break;
    case V_RED:
        if (ntok != 5) return -1;
        if (parse_u64(tok[1], &in->v[0]) || parse_u64(tok[2], &in->v[1]) ||
            parse_u64(tok[3], &in->v[2])) return -1;
        if (strcmp(tok[4], "add") == 0) in->redop = 0;
        else if (strcmp(tok[4], "max") == 0) in->redop = 1;
        else if (strcmp(tok[4], "min") == 0) in->redop = 2;
        else return -1;
        break;
    case V_SET:
        if (ntok != 4) return -1;
        if (parse_u64(tok[1], &in->v[0]) || parse_u64(tok[2], &in->v[1]) ||
            parse_u64(tok[3], &in->v[2])) return -1;
        break;
    case V_CMP:
        if (ntok != 5) return -1;
        if (parse_u64(tok[1], &in->v[0]) || parse_u64(tok[2], &in->v[1]) ||
            parse_u64(tok[3], &in->v[2])) return -1;
        if (strcmp(tok[4], "eq") == 0 || strcmp(tok[4], "==") == 0) in->redop = 0;
        else if (strcmp(tok[4], "ne") == 0 || strcmp(tok[4], "!=") == 0) in->redop = 1;
        else if (strcmp(tok[4], "lt") == 0 || strcmp(tok[4], "<") == 0) in->redop = 2;
        else if (strcmp(tok[4], "le") == 0 || strcmp(tok[4], "<=") == 0) in->redop = 3;
        else if (strcmp(tok[4], "gt") == 0 || strcmp(tok[4], ">") == 0) in->redop = 4;
        else if (strcmp(tok[4], "ge") == 0 || strcmp(tok[4], ">=") == 0) in->redop = 5;
        else return -1;
        break;
    case V_JZ:
        if (ntok != 3) return -1;
        if (parse_u64(tok[1], &in->v[0]) || parse_u64(tok[2], &in->v[1])) return -1;
        break;
    case V_JMP:
        if (ntok != 2) return -1;
        if (parse_u64(tok[1], &in->v[0])) return -1;
        break;
    default: return -1;
    }
    return 0;
}

int veg_parse_file(FILE *f, veg_instr_t *prog, size_t max, size_t *out_n)
{
    char line[512];
    size_t n = 0;
    if (!f || !prog || !out_n) return -1;
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

static const char *op_name(int op)
{
    switch (op) {
    case V_LD: return "ld"; case V_ST: return "st";
    case V_ADD: return "add"; case V_SUB: return "sub"; case V_MUL: return "mul";
    case V_RED: return "red"; case V_CMP: return "cmp"; case V_SET: return "set";
    case V_JZ: return "jz"; case V_JMP: return "jmp"; case V_EFFECT: return "effect";
    default: return "???";
    }
}

int veg_disasm(const veg_instr_t *in, char *buf, size_t buflen)
{
    if (!in || !buf || buflen == 0) return -1;
    switch (in->op) {
    case V_EFFECT:
        return snprintf(buf, buflen, "effect");
    case V_LD: case V_ST:
        return snprintf(buf, buflen, "%s 0x%llx, 0x%llx, %llu", op_name(in->op),
            (unsigned long long)in->v[0], (unsigned long long)in->v[1],
            (unsigned long long)in->v[2]);
    case V_ADD: case V_SUB: case V_MUL:
        return snprintf(buf, buflen, "%s 0x%llx, 0x%llx, 0x%llx, %llu", op_name(in->op),
            (unsigned long long)in->v[0], (unsigned long long)in->v[1],
            (unsigned long long)in->v[2], (unsigned long long)in->v[3]);
    case V_RED:
        return snprintf(buf, buflen, "red 0x%llx, 0x%llx, %llu, %d",
            (unsigned long long)in->v[0], (unsigned long long)in->v[1],
            (unsigned long long)in->v[2], in->redop);
    case V_SET:
        return snprintf(buf, buflen, "set 0x%llx, %llu, %llu",
            (unsigned long long)in->v[0], (unsigned long long)in->v[1],
            (unsigned long long)in->v[2]);
    case V_CMP:
        return snprintf(buf, buflen, "cmp 0x%llx, 0x%llx, 0x%llx, %d",
            (unsigned long long)in->v[0], (unsigned long long)in->v[1],
            (unsigned long long)in->v[2], in->redop);
    case V_JZ:
        return snprintf(buf, buflen, "jz 0x%llx, %llu",
            (unsigned long long)in->v[0], (unsigned long long)in->v[1]);
    case V_JMP:
        if (in->v[0] == ~(uint64_t)0)
            return snprintf(buf, buflen, "jmp *mem[0xF000]  ; ret");
        return snprintf(buf, buflen, "jmp %llu", (unsigned long long)in->v[0]);
    default:
        return snprintf(buf, buflen, "???");
    }
}

#define VEG_RET_PC_ADDR 0xF000ull
#define VEG_JMP_INDIRECT_RET (~(uint64_t)0)

static int exec_one(const veg_instr_t *in, uint64_t *mem, size_t mem_size,
                    size_t n_prog, size_t *pc,
                    veg_host_fn host, void *host_ud)
{
    switch (in->op) {
    case V_EFFECT:
        if (!host) return -1;
        if (host(mem, mem_size, host_ud) != 0)
            return 1; /* host requested stop */
        break;
    case V_SET: {
        uint64_t dst = in->v[0], imm = in->v[1], len = in->v[2];
        if (len && dst + len > mem_size) return -1;
        for (uint64_t k = 0; k < len; k++) mem[dst + k] = imm;
        break;
    }
    case V_JMP: {
        uint64_t target = in->v[0];
        if (target == VEG_JMP_INDIRECT_RET) {
            if (VEG_RET_PC_ADDR >= mem_size) return -1;
            target = mem[VEG_RET_PC_ADDR];
        }
        if (target > n_prog) return -1;
        *pc = (size_t)target;
        return 0;
    }
    case V_JZ:
        if (in->v[0] >= mem_size || in->v[1] > n_prog) return -1;
        if (mem[in->v[0]] == 0) { *pc = (size_t)in->v[1]; return 0; }
        break;
    case V_CMP: {
        uint64_t f = in->v[0], a = in->v[1], b = in->v[2];
        if (f >= mem_size || a >= mem_size || b >= mem_size) return -1;
        uint64_t x = mem[a], y = mem[b];
        int r = 0;
        switch (in->redop) {
        case 0: r = (x == y); break; case 1: r = (x != y); break;
        case 2: r = (x < y); break; case 3: r = (x <= y); break;
        case 4: r = (x > y); break; case 5: r = (x >= y); break;
        default: return -1;
        }
        mem[f] = r ? 1ull : 0ull;
        break;
    }
    case V_LD: case V_ST: {
        uint64_t dst = in->v[0], src = in->v[1], len = in->v[2];
        if (len && (dst + len > mem_size || src + len > mem_size)) return -1;
        if (dst <= src)
            for (uint64_t k = 0; k < len; k++) mem[dst + k] = mem[src + k];
        else
            for (uint64_t k = len; k-- > 0; ) mem[dst + k] = mem[src + k];
        break;
    }
    case V_ADD: case V_SUB: case V_MUL: {
        uint64_t dst = in->v[0], a = in->v[1], b = in->v[2], len = in->v[3];
        if (len && (dst + len > mem_size || a + len > mem_size || b + len > mem_size))
            return -1;
        for (uint64_t k = 0; k < len; k++) {
            uint64_t x = mem[a + k], y = mem[b + k];
            mem[dst + k] = in->op == V_ADD ? x + y : in->op == V_SUB ? x - y : x * y;
        }
        break;
    }
    case V_RED: {
        uint64_t dst = in->v[0], a = in->v[1], len = in->v[2];
        if (dst >= mem_size || (len && a + len > mem_size) || len == 0) return -1;
        uint64_t acc = mem[a];
        for (uint64_t k = 1; k < len; k++) {
            uint64_t v = mem[a + k];
            if (in->redop == 1) { if (v > acc) acc = v; }
            else if (in->redop == 2) { if (v < acc) acc = v; }
            else acc += v;
        }
        mem[dst] = acc;
        break;
    }
    default: return -1;
    }
    (*pc)++;
    return 0;
}

int veg_step(const veg_instr_t *prog, size_t n,
             uint64_t *mem, size_t mem_size, size_t *pc)
{
    if (!prog || !mem || !pc) return -1;
    if (*pc >= n) return 1;
    return exec_one(&prog[*pc], mem, mem_size, n, pc, NULL, NULL);
}

int veg_step_host(const veg_instr_t *prog, size_t n,
                  uint64_t *mem, size_t mem_size, size_t *pc,
                  veg_host_fn host, void *userdata)
{
    if (!prog || !mem || !pc) return -1;
    if (*pc >= n) return 1;
    return exec_one(&prog[*pc], mem, mem_size, n, pc, host, userdata);
}

int veg_exec(const veg_instr_t *prog, size_t n,
             uint64_t *mem, size_t mem_size)
{
    size_t pc = 0;
    if (!prog || !mem) return -1;
    while (pc < n) {
        int r = exec_one(&prog[pc], mem, mem_size, n, &pc, NULL, NULL);
        if (r < 0) return -1;
        if (r > 0) return 0;
    }
    return 0;
}

int veg_exec_host(const veg_instr_t *prog, size_t n,
                  uint64_t *mem, size_t mem_size,
                  veg_host_fn host, void *userdata)
{
    size_t pc = 0;
    if (!prog || !mem) return -1;
    while (pc < n) {
        int r = exec_one(&prog[pc], mem, mem_size, n, &pc, host, userdata);
        if (r < 0) return -1;
        if (r > 0) return 0; /* host stop */
    }
    return 0;
}

int veg_exec_trace(const veg_instr_t *prog, size_t n,
                   uint64_t *mem, size_t mem_size,
                   veg_trace_fn trace, void *userdata)
{
    size_t pc = 0;
    if (!prog || !mem) return -1;
    while (pc < n) {
        if (trace && trace(pc, &prog[pc], mem, mem_size, userdata))
            return 0;
        if (exec_one(&prog[pc], mem, mem_size, n, &pc, NULL, NULL) != 0)
            return -1;
    }
    return 0;
}
