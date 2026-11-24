#include "veg3.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Lexer                                                                */
/* ------------------------------------------------------------------ */

#define TOK_MAX  1024
#define TOK_LEN    64

typedef struct {
    char toks[TOK_MAX][TOK_LEN];
    int  n, pos;
} toks_t;

static int lex(const char *text, toks_t *out)
{
    out->n = out->pos = 0;
    const char *p = text;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (*p == '#') { while (*p && *p != '\n') p++; continue; }
        if (out->n >= TOK_MAX) return -1;
        if (p[0] == '-' && p[1] == '>') {
            snprintf(out->toks[out->n++], TOK_LEN, "->");
            p += 2; continue;
        }
        if (strchr("(){};=<>[]!,.:", *p)) {
            char c = *p++;
            char *t = out->toks[out->n];
            int k = 0;
            t[k++] = c;
            if ((c == '=' && *p == '=') ||
                ((c == '<' || c == '>' || c == '!') && *p == '='))
                t[k++] = *p++;
            t[k] = '\0';
            out->n++; continue;
        }
        size_t k = 0;
        while (*p && !isspace((unsigned char)*p) &&
               !strchr("(){};=<>[]!,.:-", *p) && k < TOK_LEN - 1)
            out->toks[out->n][k++] = *p++;
        out->toks[out->n][k] = '\0';
        out->n++;
    }
    return 0;
}

static const char *peek(toks_t *t) {
    return t->pos < t->n ? t->toks[t->pos] : NULL;
}
static const char *peek_n(toks_t *t, int off) {
    int i = t->pos + off;
    return i < t->n ? t->toks[i] : NULL;
}
static const char *next(toks_t *t) {
    const char *s = peek(t);
    if (s) t->pos++;
    return s;
}
static int accept(toks_t *t, const char *s) {
    if (peek(t) && strcmp(peek(t), s) == 0) { next(t); return 1; }
    return 0;
}
static int expect(toks_t *t, const char *s) {
    return accept(t, s) ? 0 : -1;
}
static int is_ident(const char *s) {
    if (!s || !*s) return 0;
    if (!isalpha((unsigned char)*s) && *s != '_') return 0;
    for (const char *c = s + 1; *c; c++)
        if (!isalnum((unsigned char)*c) && *c != '_') return 0;
    return 1;
}
static int is_num(const char *s) {
    if (!s || !*s) return 0;
    const char *p = s;
    if (*p == '+' || *p == '-') p++;
    if (!*p) return 0;
    for (; *p; p++) if (!isdigit((unsigned char)*p)) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Emit helpers                                                         */
/* ------------------------------------------------------------------ */

static int emit(veg3_unit_t *u, veg_instr_t in)
{
    if (!u->prog || u->prog_len >= u->prog_cap) return -1;
    u->prog[u->prog_len++] = in;
    return 0;
}
static int emit_ld(veg3_unit_t *u, uint64_t dst, uint64_t src, uint64_t n)
{
    veg_instr_t in; memset(&in, 0, sizeof in);
    in.op = V_LD; in.v[0] = dst; in.v[1] = src; in.v[2] = n;
    return emit(u, in);
}
static int emit_set(veg3_unit_t *u, uint64_t dst, uint64_t imm, uint64_t n)
{
    veg_instr_t in; memset(&in, 0, sizeof in);
    in.op = V_SET; in.v[0] = dst; in.v[1] = imm; in.v[2] = n;
    return emit(u, in);
}
static int emit_binop(veg3_unit_t *u, int op, uint64_t dst,
                      uint64_t a, uint64_t b, uint64_t n)
{
    veg_instr_t in; memset(&in, 0, sizeof in);
    in.op = op; in.v[0] = dst; in.v[1] = a; in.v[2] = b; in.v[3] = n;
    return emit(u, in);
}
static int emit_jmp(veg3_unit_t *u, uint64_t target)
{
    veg_instr_t in; memset(&in, 0, sizeof in);
    in.op = V_JMP; in.v[0] = target;
    return emit(u, in);
}

int veg3_emit_call(veg3_unit_t *u, uint32_t func_idx,
                   const uint64_t *arg_bases, const uint32_t *arg_sizes,
                   uint32_t nargs)
{
    if (!u || func_idx >= u->nfuncs) return -1;
    const veg3_func_t *fn = &u->funcs[func_idx];
    if (nargs != fn->nparams) return -1;

    uint64_t cursor = VEG3_ARG_BASE;
    for (uint32_t i = 0; i < nargs; i++) {
        if (arg_sizes[i] &&
            emit_ld(u, cursor, arg_bases[i], arg_sizes[i]) != 0)
            return -1;
        cursor += arg_sizes[i];
    }

    size_t set_idx = u->prog_len;
    if (emit_set(u, VEG3_RET_PC_ADDR, 0, 1) != 0) return -1;
    if (emit_jmp(u, fn->entry_pc) != 0) return -1;
    u->prog[set_idx].v[1] = (uint64_t)u->prog_len;
    return 0;
}

int veg3_emit_ret(veg3_unit_t *u, uint64_t retval_base, uint32_t retval_size)
{
    if (!u) return -1;
    if (retval_size > 0) {
        if (emit_ld(u, VEG3_RETVAL_BASE, retval_base, retval_size) != 0)
            return -1;
    }
    return emit_jmp(u, ~(uint64_t)0);
}

/* ------------------------------------------------------------------ */
/* Symbol / type tables                                                 */
/* ------------------------------------------------------------------ */

#define LOC_MAX 64

typedef struct {
    char     name[VEG3_NAME_LEN];
    uint64_t base;
    uint32_t size;
    int      struct_idx; /* -1 if scalar */
} loc_t;

typedef struct {
    loc_t    locs[LOC_MAX];
    uint32_t n;
    uint64_t next;
} frame_t;

static int find_struct(const veg3_unit_t *u, const char *name)
{
    for (uint32_t i = 0; i < u->nstructs; i++)
        if (strcmp(u->structs[i].name, name) == 0) return (int)i;
    return -1;
}
static int find_func(const veg3_unit_t *u, const char *name)
{
    for (uint32_t i = 0; i < u->nfuncs; i++)
        if (strcmp(u->funcs[i].name, name) == 0) return (int)i;
    return -1;
}
static int find_loc(const frame_t *f, const char *name)
{
    for (uint32_t i = 0; i < f->n; i++)
        if (strcmp(f->locs[i].name, name) == 0) return (int)i;
    return -1;
}

static int parse_type(veg3_unit_t *u, const char *tname, veg3_type_t *out)
{
    memset(out, 0, sizeof *out);
    snprintf(out->name, sizeof out->name, "%s", tname);
    if (strcmp(tname, "v1") == 0) { out->size = 1; return 0; }
    if (strcmp(tname, "v8") == 0) { out->size = 8; return 0; }
    if (strcmp(tname, "unit") == 0 || strcmp(tname, "void") == 0) {
        out->size = 0; return 0;
    }
    int idx = find_struct(u, tname);
    if (idx >= 0) {
        out->size = u->structs[idx].size;
        out->is_struct = 1;
        out->struct_idx = idx;
        return 0;
    }
    return -1;
}

static int frame_alloc(frame_t *f, const char *name, uint32_t size, int sidx)
{
    if (f->n >= LOC_MAX || find_loc(f, name) >= 0) return -1;
    loc_t *l = &f->locs[f->n++];
    snprintf(l->name, sizeof l->name, "%s", name);
    l->base = f->next;
    l->size = size;
    l->struct_idx = sidx;
    f->next += size ? size : 1;
    return 0;
}

/* Resolve path: name or name.field.field → base, size */
static int resolve_path(veg3_unit_t *u, frame_t *f, toks_t *t,
                        uint64_t *base_out, uint32_t *size_out)
{
    const char *name = next(t);
    if (!is_ident(name)) return -1;
    int li = find_loc(f, name);
    if (li < 0) return -1;
    uint64_t base = f->locs[li].base;
    uint32_t size = f->locs[li].size;
    int sidx = f->locs[li].struct_idx;

    while (accept(t, ".")) {
        const char *fld = next(t);
        if (!is_ident(fld) || sidx < 0) return -1;
        const veg3_struct_t *s = &u->structs[sidx];
        int found = -1;
        for (uint32_t i = 0; i < s->nfields; i++)
            if (strcmp(s->fields[i].name, fld) == 0) { found = (int)i; break; }
        if (found < 0) return -1;
        base += s->fields[found].offset;
        size  = s->fields[found].size;
        sidx  = s->fields[found].struct_idx;
    }
    *base_out = base;
    *size_out = size;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Struct parsing                                                       */
/* ------------------------------------------------------------------ */

static int parse_struct(toks_t *t, veg3_unit_t *u)
{
    if (!accept(t, "struct")) return -1;
    const char *name = next(t);
    if (!is_ident(name)) return -1;
    int idx = find_struct(u, name);
    if (idx < 0) {
        if (u->nstructs >= VEG3_MAX_STRUCTS) return -1;
        idx = (int)u->nstructs;
        u->nstructs++;
    }
    if (expect(t, "{") != 0) return -1;

    veg3_struct_t *s = &u->structs[idx];
    memset(s, 0, sizeof *s);
    snprintf(s->name, sizeof s->name, "%s", name);

    uint32_t offset = 0;
    while (peek(t) && !accept(t, "}")) {
        if (s->nfields >= VEG3_MAX_FIELDS) return -1;
        const char *fname = next(t);
        if (!is_ident(fname) || expect(t, ":") != 0) return -1;
        const char *tname = next(t);
        if (!tname) return -1;
        veg3_type_t ty;
        if (parse_type(u, tname, &ty) != 0) return -1;
        veg3_field_t *f = &s->fields[s->nfields++];
        snprintf(f->name, sizeof f->name, "%s", fname);
        f->offset = offset;
        f->size = ty.size;
        f->struct_idx = ty.is_struct ? ty.struct_idx : -1;
        offset += ty.size;
        accept(t, ";");
    }
    s->size = offset;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Expression + statement lowering (function body)                      */
/* ------------------------------------------------------------------ */

/* Parse a primary: number | path | call
 * Leaves result in *base/*size (allocated in frame if needed). */
static int lower_expr(toks_t *t, veg3_unit_t *u, frame_t *f,
                      uint64_t *base_out, uint32_t *size_out);

static int lower_call(toks_t *t, veg3_unit_t *u, frame_t *f,
                      const char *fname,
                      uint64_t *base_out, uint32_t *size_out)
{
    int fi = find_func(u, fname);
    if (fi < 0) return -1;
    veg3_func_t *fn = &u->funcs[fi];
    if (expect(t, "(") != 0) return -1;

    uint64_t abases[VEG3_MAX_PARAMS];
    uint32_t asizes[VEG3_MAX_PARAMS];
    uint32_t nargs = 0;

    if (!accept(t, ")")) {
        for (;;) {
            if (nargs >= VEG3_MAX_PARAMS) return -1;
            uint64_t b; uint32_t s;
            if (lower_expr(t, u, f, &b, &s) != 0) return -1;
            abases[nargs] = b;
            asizes[nargs] = s;
            nargs++;
            if (accept(t, ")")) break;
            if (expect(t, ",") != 0) return -1;
        }
    }
    if (veg3_emit_call(u, (uint32_t)fi, abases, asizes, nargs) != 0)
        return -1;

    /* Result lives at RETVAL_BASE */
    *base_out = VEG3_RETVAL_BASE;
    *size_out = fn->ret.size;
    return 0;
}

static int lower_expr(toks_t *t, veg3_unit_t *u, frame_t *f,
                      uint64_t *base_out, uint32_t *size_out)
{
    const char *tok = peek(t);
    if (!tok) return -1;

    /* integer literal → allocate temp, set */
    if (is_num(tok)) {
        next(t);
        uint64_t imm = strtoull(tok, NULL, 0);
        if (frame_alloc(f, "", 1, -1) != 0) return -1; /* anonymous */
        /* reuse last loc */
        loc_t *l = &f->locs[f->n - 1];
        snprintf(l->name, sizeof l->name, "$t%u", f->n);
        if (emit_set(u, l->base, imm, 1) != 0) return -1;
        *base_out = l->base;
        *size_out = 1;
        return 0;
    }

    if (!is_ident(tok)) return -1;

    /* call?  name( */
    if (peek_n(t, 1) && strcmp(peek_n(t, 1), "(") == 0) {
        const char *fname = next(t);
        return lower_call(t, u, f, fname, base_out, size_out);
    }

    /* path */
    return resolve_path(u, f, t, base_out, size_out);
}

static int lower_stmt(toks_t *t, veg3_unit_t *u, frame_t *f, veg3_func_t *fn);

static int lower_block(toks_t *t, veg3_unit_t *u, frame_t *f, veg3_func_t *fn)
{
    if (expect(t, "{") != 0) return -1;
    while (peek(t) && strcmp(peek(t), "}") != 0)
        if (lower_stmt(t, u, f, fn) != 0) return -1;
    if (expect(t, "}") != 0) return -1;
    return 0;
}

static int lower_stmt(toks_t *t, veg3_unit_t *u, frame_t *f, veg3_func_t *fn)
{
    const char *tok = peek(t);
    if (!tok) return -1;

    /* local decl: v1 x;  /  v8 x;  /  Point p; */
    if (strcmp(tok, "v1") == 0 || strcmp(tok, "v8") == 0 ||
        find_struct(u, tok) >= 0) {
        const char *tname = next(t);
        veg3_type_t ty;
        if (parse_type(u, tname, &ty) != 0) return -1;
        const char *vname = next(t);
        if (!is_ident(vname)) return -1;
        int sidx = ty.is_struct ? ty.struct_idx : -1;
        if (frame_alloc(f, vname, ty.size ? ty.size : 1, sidx) != 0)
            return -1;
        /* optional init: = expr */
        if (accept(t, "=")) {
            uint64_t rb; uint32_t rs;
            if (lower_expr(t, u, f, &rb, &rs) != 0) return -1;
            loc_t *l = &f->locs[find_loc(f, vname)];
            uint32_t n = rs < l->size ? rs : l->size;
            if (n && emit_ld(u, l->base, rb, n) != 0) return -1;
        }
        accept(t, ";");
        return 0;
    }

    /* return;  /  return expr; */
    if (strcmp(tok, "return") == 0) {
        next(t);
        if (peek(t) && strcmp(peek(t), ";") != 0 &&
            strcmp(peek(t), "}") != 0) {
            uint64_t rb; uint32_t rs;
            if (lower_expr(t, u, f, &rb, &rs) != 0) return -1;
            uint32_t n = fn->has_ret ? fn->ret.size : 0;
            if (n > rs) n = rs;
            if (veg3_emit_ret(u, rb, n) != 0) return -1;
        } else {
            if (veg3_emit_ret(u, 0, 0) != 0) return -1;
        }
        accept(t, ";");
        return 0;
    }

    /* block */
    if (strcmp(tok, "{") == 0)
        return lower_block(t, u, f, fn);

    /* assignment: path = expr;   or   path = expr op expr; */
    if (is_ident(tok)) {
        /* could be call-as-statement */
        if (peek_n(t, 1) && strcmp(peek_n(t, 1), "(") == 0) {
            uint64_t b; uint32_t s;
            if (lower_expr(t, u, f, &b, &s) != 0) return -1;
            accept(t, ";");
            return 0;
        }

        uint64_t lbase; uint32_t lsize;
        /* need to resolve path without consuming for binop check —
           resolve_path consumes. Save pos. */
        int saved = t->pos;
        if (resolve_path(u, f, t, &lbase, &lsize) != 0) return -1;

        if (!accept(t, "=")) {
            t->pos = saved;
            return -1;
        }

        /* rhs: expr  or  expr op expr */
        uint64_t abase; uint32_t asize;
        if (lower_expr(t, u, f, &abase, &asize) != 0) return -1;

        const char *op = peek(t);
        if (op && (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 ||
                   strcmp(op, "*") == 0)) {
            next(t);
            uint64_t bbase; uint32_t bsize;
            if (lower_expr(t, u, f, &bbase, &bsize) != 0) return -1;
            int vop = strcmp(op, "+") == 0 ? V_ADD
                    : strcmp(op, "-") == 0 ? V_SUB : V_MUL;
            uint32_t n = lsize;
            if (asize < n) n = asize;
            if (bsize < n) n = bsize;
            if (n == 0) n = 1;
            if (emit_binop(u, vop, lbase, abase, bbase, n) != 0) return -1;
        } else {
            uint32_t n = asize < lsize ? asize : lsize;
            if (n && emit_ld(u, lbase, abase, n) != 0) return -1;
        }
        accept(t, ";");
        return 0;
    }

    return -1;
}

/* ------------------------------------------------------------------ */
/* Function parsing                                                     */
/* ------------------------------------------------------------------ */

static int parse_fn(toks_t *t, veg3_unit_t *u)
{
    if (!accept(t, "fn")) return -1;
    const char *name = next(t);
    if (!is_ident(name) || find_func(u, name) >= 0) return -1;
    if (u->nfuncs >= VEG3_MAX_FUNCS) return -1;
    if (expect(t, "(") != 0) return -1;

    veg3_func_t *fn = &u->funcs[u->nfuncs];
    memset(fn, 0, sizeof *fn);
    snprintf(fn->name, sizeof fn->name, "%s", name);

    if (!accept(t, ")")) {
        for (;;) {
            if (fn->nparams >= VEG3_MAX_PARAMS) return -1;
            const char *pname = next(t);
            if (!is_ident(pname) || expect(t, ":") != 0) return -1;
            const char *tname = next(t);
            if (!tname) return -1;
            veg3_type_t ty;
            if (parse_type(u, tname, &ty) != 0) return -1;
            snprintf(fn->param_names[fn->nparams], VEG3_NAME_LEN, "%s", pname);
            fn->params[fn->nparams] = ty;
            fn->nparams++;
            if (accept(t, ")")) break;
            if (expect(t, ",") != 0) return -1;
        }
    }

    fn->has_ret = 0;
    fn->ret.size = 0;
    if (accept(t, "->")) {
        const char *rname = next(t);
        if (!rname || parse_type(u, rname, &fn->ret) != 0) return -1;
        fn->has_ret = (fn->ret.size > 0);
    }

    /* entry */
    fn->entry_pc = (uint32_t)u->prog_len;

    /* Build frame: params live at ARG_BASE */
    frame_t frame;
    memset(&frame, 0, sizeof frame);
    frame.next = 0x300; /* locals start here */

    uint64_t arg_cursor = VEG3_ARG_BASE;
    for (uint32_t i = 0; i < fn->nparams; i++) {
        if (frame.n >= LOC_MAX) return -1;
        loc_t *l = &frame.locs[frame.n++];
        snprintf(l->name, sizeof l->name, "%s", fn->param_names[i]);
        l->base = arg_cursor;
        l->size = fn->params[i].size ? fn->params[i].size : 1;
        l->struct_idx = fn->params[i].is_struct ? fn->params[i].struct_idx : -1;
        arg_cursor += l->size;
    }

    if (lower_block(t, u, &frame, fn) != 0) return -1;

    /* Ensure function ends with ret */
    if (u->prog_len == 0 ||
        u->prog[u->prog_len - 1].op != V_JMP ||
        u->prog[u->prog_len - 1].v[0] != ~(uint64_t)0) {
        if (veg3_emit_ret(u, 0, 0) != 0) return -1;
    }

    fn->frame_size = (uint32_t)(frame.next - 0x300);
    u->nfuncs++;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Compile entry                                                        */
/* ------------------------------------------------------------------ */

int veg3_compile(const char *src, veg3_unit_t *unit)
{
    if (!src || !unit) return -1;

    veg_instr_t *prog = unit->prog;
    size_t cap = unit->prog_cap;
    veg2_var_t *vars = unit->vars;
    size_t vcap = unit->vars_cap;

    memset(unit, 0, sizeof *unit);
    unit->prog = prog;
    unit->prog_cap = cap;
    unit->vars = vars;
    unit->vars_cap = vcap;
    unit->next_addr = 0x100;

    toks_t t;
    if (lex(src, &t) != 0) return -1;

    /* Pass 0: register struct names (struct fields may reference any
       struct, even one defined later in the unit). */
    {
        toks_t tp;
        if (lex(src, &tp) != 0) return -1;
        while (peek(&tp)) {
            if (strcmp(peek(&tp), "struct") == 0) {
                next(&tp);
                const char *nm = next(&tp);
                if (!is_ident(nm)) return -1;
                if (find_struct(unit, nm) >= 0) return -1;
                if (unit->nstructs >= VEG3_MAX_STRUCTS) return -1;
                veg3_struct_t *st = &unit->structs[unit->nstructs++];
                memset(st, 0, sizeof *st);
                snprintf(st->name, sizeof st->name, "%s", nm);
            } else {
                next(&tp);
            }
        }
    }

    /* Pass 1: structs only (so types resolve) */
    int saved = t.pos;
    while (peek(&t)) {
        if (strcmp(peek(&t), "struct") == 0) {
            if (parse_struct(&t, unit) != 0) return -1;
        } else {
            /* skip fn bodies in pass 1 — re-lex approach: advance by braces */
            if (strcmp(peek(&t), "fn") == 0) {
                next(&t); /* fn */
                next(&t); /* name */
                /* skip until matching closing brace of body */
                int depth = 0; int seen_body = 0;
                while (peek(&t)) {
                    const char *tk = next(&t);
                    if (strcmp(tk, "{") == 0) { depth++; seen_body = 1; }
                    else if (strcmp(tk, "}") == 0) {
                        depth--;
                        if (seen_body && depth == 0) break;
                    }
                }
            } else {
                next(&t);
            }
        }
    }

    /* Pass 2: functions with full lowering */
    t.pos = saved;
    /* Re-lex is cleaner */
    if (lex(src, &t) != 0) return -1;
    /* Reset func table but keep structs */
    uint32_t ns = unit->nstructs;
    veg3_struct_t structs_copy[VEG3_MAX_STRUCTS];
    memcpy(structs_copy, unit->structs, sizeof structs_copy);
    size_t plen = unit->prog_len;
    unit->nfuncs = 0;
    unit->prog_len = plen; /* should be 0 */
    unit->nstructs = ns;
    memcpy(unit->structs, structs_copy, sizeof structs_copy);

    while (peek(&t)) {
        if (strcmp(peek(&t), "struct") == 0) {
            /* already collected — skip */
            next(&t); next(&t);
            int depth = 0;
            while (peek(&t)) {
                const char *tk = next(&t);
                if (strcmp(tk, "{") == 0) depth++;
                else if (strcmp(tk, "}") == 0) { depth--; if (depth == 0) break; }
            }
            continue;
        }
        if (strcmp(peek(&t), "fn") == 0) {
            if (parse_fn(&t, unit) != 0) return -1;
            continue;
        }
        return -1;
    }
    return 0;
}

void veg3_dump(const veg3_unit_t *u, FILE *out)
{
    if (!u || !out) return;
    fprintf(out, "=== Veggdor V3 unit ===\n");
    fprintf(out, "  RET_PC=0x%llx  ARG=0x%llx  RETVAL=0x%llx\n",
            (unsigned long long)VEG3_RET_PC_ADDR,
            (unsigned long long)VEG3_ARG_BASE,
            (unsigned long long)VEG3_RETVAL_BASE);
    fprintf(out, "Structs: %u\n", u->nstructs);
    for (uint32_t i = 0; i < u->nstructs; i++) {
        const veg3_struct_t *s = &u->structs[i];
        fprintf(out, "  struct %s (%u lanes)\n", s->name, s->size);
        for (uint32_t j = 0; j < s->nfields; j++)
            fprintf(out, "    .%s off=%u sz=%u\n",
                    s->fields[j].name, s->fields[j].offset, s->fields[j].size);
    }
    fprintf(out, "Functions: %u\n", u->nfuncs);
    for (uint32_t i = 0; i < u->nfuncs; i++) {
        const veg3_func_t *f = &u->funcs[i];
        fprintf(out, "  fn %s entry=%u params=%u ret_sz=%u\n",
                f->name, f->entry_pc, f->nparams, f->ret.size);
    }
    fprintf(out, "Lowered instructions: %zu\n", u->prog_len);
    for (size_t i = 0; i < u->prog_len; i++) {
        char buf[128];
        veg_disasm(&u->prog[i], buf, sizeof buf);
        fprintf(out, "  %04zu  %s\n", i, buf);
    }
}
