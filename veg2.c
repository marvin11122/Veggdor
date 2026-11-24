#include "veg2.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define V2_TOK_MAX 64
#define V2_TOK_LEN 32

typedef struct {
    char toks[V2_TOK_MAX][V2_TOK_LEN];
    int n;
} v2toks;

typedef struct {
    v2toks t;
    int pos;
    veg_instr_t *prog;
    size_t len, cap;
    veg2_var_t *vars;
    size_t vars_len, vars_cap;
    uint64_t next_addr;
    uint64_t next_flag;
} p2_t;

static int is_ident(const char *s) {
    if (!s || !*s) return 0;
    for (const char *c = s; *c; c++)
        if (!isalnum((unsigned char)*c) && *c != '_') return 0;
    return 1;
}

static int is_num(const char *s) {
    if (!s || !*s) return 0;
    const char *p = s;
    if (*p == '-' || *p == '+') p++;
    if (!*p) return 0;
    for (; *p; p++)
        if (!isdigit((unsigned char)*p)) return 0;
    return 1;
}

static const char *peek_at(p2_t *p, int off) {
    int i = p->pos + off;
    return i < p->t.n ? p->t.toks[i] : NULL;
}

static const char *peek(p2_t *p) { return peek_at(p, 0); }

static const char *next(p2_t *p) {
    const char *s = peek(p);
    if (s) p->pos++;
    return s;
}

static const char *expect(p2_t *p, const char *s) {
    const char *t = next(p);
    if (!t || strcmp(t, s) != 0) return NULL;
    return t;
}

static int lex(const char *text, v2toks *out) {
    out->n = 0;
    const char *p = text;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }
        if (out->n >= V2_TOK_MAX) return -1;
        if (strchr("(){};=<>[]!,", *p)) {
            char c = *p++;
            char *t = out->toks[out->n];
            int k = 0;
            t[k++] = c;
            if ((c == '=' && *p == '=') ||
                ((c == '<' || c == '>' || c == '!') && *p == '='))
                t[k++] = *p++;
            t[k] = '\0';
            out->n++;
            continue;
        }
        size_t k = 0;
        while (*p && !isspace((unsigned char)*p) &&
               strchr("(){};=<>[]!,", *p) == NULL && k < V2_TOK_LEN - 1)
            out->toks[out->n][k++] = *p++;
        out->toks[out->n][k] = '\0';
        out->n++;
    }
    return 0;
}

static int var_addr(p2_t *p, const char *name, uint64_t *out) {
    for (size_t i = 0; i < p->vars_len; i++)
        if (strcmp(p->vars[i].name, name) == 0) {
            *out = p->vars[i].addr;
            return 0;
        }
    return -1;
}

static char v2_name_pool[8192];
static size_t v2_name_pool_used;

static const char *pool_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    if (v2_name_pool_used + n > sizeof v2_name_pool) return NULL;
    char *r = v2_name_pool + v2_name_pool_used;
    memcpy(r, s, n);
    v2_name_pool_used += n;
    return r;
}

static int declare(p2_t *p, const char *name) {
    uint64_t dummy = 0;
    if (!is_ident(name) || var_addr(p, name, &dummy) == 0) return -1;
    if (p->vars_len >= p->vars_cap) return -1;
    const char *copy = pool_strdup(name);
    if (!copy) return -1;
    if (p->vars) {
        p->vars[p->vars_len].name = copy;
        p->vars[p->vars_len].addr = p->next_addr;
    }
    p->vars_len++;
    p->next_addr += 8;
    return 0;
}

static int emit(p2_t *p, veg_instr_t in) {
    if (p->len >= p->cap) return -1;
    p->prog[p->len] = in;
    return (int)p->len++;
}

static int parse_stmt(p2_t *p);
static int parse_stmt_or_block(p2_t *p);
static int parse_init(p2_t *p, const char *name);

static int parse_cond(p2_t *p, uint64_t *flag_out) {
    const char *a = next(p);
    const char *op = next(p);
    int redop = -1;
    if (a && op) {
        if (strcmp(op, "==") == 0) redop = 0;
        else if (strcmp(op, "!=") == 0) redop = 1;
        else if (strcmp(op, "<") == 0) redop = 2;
        else if (strcmp(op, "<=") == 0) redop = 3;
        else if (strcmp(op, ">") == 0) redop = 4;
        else if (strcmp(op, ">=") == 0) redop = 5;
    }
    if (redop < 0) return -1;
    const char *b = next(p);
    if (!b || !is_ident(a) || !is_ident(b)) return -1;
    uint64_t va = 0, vb = 0;
    if (var_addr(p, a, &va) || var_addr(p, b, &vb)) return -1;
    uint64_t f = p->next_flag++;
    int idx = emit(p, (veg_instr_t){ .op = V_CMP, .v = { f, va, vb }, .redop = redop });
    if (idx < 0) return -1;
    *flag_out = f;
    return 0;
}

static int parse_if(p2_t *p) {
    uint64_t flag = 0;
    if (!expect(p, "if") || !expect(p, "(") || parse_cond(p, &flag) ||
        !expect(p, ")"))
        return -1;
    int jz = emit(p, (veg_instr_t){ .op = V_JZ, .v = { flag, 0 } });
    if (jz < 0) return -1;
    if (parse_stmt_or_block(p)) return -1;
    p->prog[jz].v[1] = p->len;
    return 0;
}

static int parse_while(p2_t *p) {
    uint64_t flag = 0;
    int top = -1;
    if (!expect(p, "while") || !expect(p, "(")) return -1;
    if (parse_cond(p, &flag)) return -1;
    top = (int)p->len - 1; /* index of emitted cmp */
    if (!expect(p, ")")) return -1;
    int jz = emit(p, (veg_instr_t){ .op = V_JZ, .v = { flag, 0 } });
    if (jz < 0) return -1;
    if (parse_stmt_or_block(p)) return -1;
    int jmp = emit(p, (veg_instr_t){ .op = V_JMP, .v = { (uint64_t)top } });
    if (jmp < 0) return -1;
    p->prog[jz].v[1] = p->len;
    return 0;
}

static int parse_stmt(p2_t *p) {
    const char *t = peek(p);
    if (!t) return -1;
    if (strcmp(t, "v8") == 0) {
        next(p);
        const char *name = next(p);
        if (!is_ident(name)) return -1;
        if (peek(p) && strcmp(peek(p), "=") == 0) {
            next(p);
            return parse_init(p, name);
        }
        if (!expect(p, ";")) return -1;
        return declare(p, name);
    }
    if (strcmp(t, "if") == 0) return parse_if(p);
    if (strcmp(t, "while") == 0) return parse_while(p);
    const char *p1 = peek_at(p, 1);
    if (is_ident(t) && p1 && strcmp(p1, "=") == 0) {
        /* assignment:  x = y;      x = y op z; */
        const char *dst_name = next(p);
        if (!expect(p, "=")) return -1;
        const char *a = next(p);
        if (!is_ident(a)) return -1;
        uint64_t vd = 0, va = 0;
        if (var_addr(p, dst_name, &vd) || var_addr(p, a, &va)) return -1;
        const char *op = peek(p);
        if (op && (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 ||
                   strcmp(op, "*") == 0)) {
            next(p);
            const char *b = next(p);
            if (!is_ident(b)) return -1;
            uint64_t vb = 0;
            if (var_addr(p, b, &vb)) return -1;
            if (!expect(p, ";")) return -1;
            int o = strcmp(op, "+") == 0 ? V_ADD : strcmp(op, "-") == 0 ? V_SUB : V_MUL;
            return emit(p, (veg_instr_t){ .op = o, .v = { vd, va, vb, 8 } }) < 0
                       ? -1
                       : 0;
        }
        if (!expect(p, ";")) return -1;
        return emit(p, (veg_instr_t){ .op = V_LD, .v = { vd, va, 8 } }) < 0 ? -1 : 0;
    }
    return -1;
}

static int parse_stmt_or_block(p2_t *p) {
    const char *t = peek(p);
    if (t && strcmp(t, "{") == 0) {
        next(p);
        while ((t = peek(p)) && strcmp(t, "}") != 0)
            if (parse_stmt(p)) return -1;
        if (!t) return -1;
        next(p);
        return 0;
    }
    return parse_stmt(p);
}

static int parse_init(p2_t *p, const char *name) {
    const char *t = peek(p);
    uint64_t addr = 0;
    if (t && strcmp(t, "[") == 0) {
        next(p);
        if (declare(p, name)) return -1;
        if (var_addr(p, name, &addr)) return -1;
        uint64_t vals[8] = { 0 };
        int n = 0;
        for (;;) {
            const char *c = peek(p);
            if (c && strcmp(c, "]") == 0) {
                next(p);
                break;
            }
            if (n >= 8) return -1;
            c = next(p);
            if (!is_num(c)) return -1;
            vals[n++] = strtoull(c, NULL, 0);
            c = peek(p);
            if (c && strcmp(c, ",") == 0) next(p);
            else if (!c || strcmp(c, "]") != 0) return -1;
        }
        if (!expect(p, ";")) return -1;
        for (int i = 0; i < n; i++) {
            int r = emit(p, (veg_instr_t){
                        .op = V_SET, .v = { addr + (uint64_t)i, vals[i], 1 } });
            if (r < 0) return -1;
        }
        return n > 0 ? 0 : -1;
    }
    if (t && is_num(t)) {
        uint64_t imm = strtoull(t, NULL, 0);
        next(p);
        if (!expect(p, ";")) return -1;
        if (declare(p, name)) return -1;
        if (var_addr(p, name, &addr)) return -1;
        if (emit(p, (veg_instr_t){ .op = V_SET, .v = { addr, imm, 8 } }) < 0)
            return -1;
        return 0;
    }
    return -1;
}

int veg2_parse(const char *text, veg_instr_t *prog, size_t max, size_t *out_n,
               veg2_var_t *vars, size_t vars_max, size_t *out_vars) {
    p2_t p;
    memset(&p, 0, sizeof p);
    v2_name_pool_used = 0;
    p.prog = prog;
    p.cap = max;
    p.vars = vars;
    p.vars_cap = vars_max;
    p.next_addr = 0x100;
    p.next_flag = 0x2000;
    if (lex(text, &p.t)) return -1;
    while (peek(&p)) {
        if (parse_stmt(&p)) return -1;
    }
    *out_n = p.len;
    *out_vars = p.vars_len;
    return 0;
}
