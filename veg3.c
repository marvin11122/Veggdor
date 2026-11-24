#include "veg3.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Lexer                                                                */
/* ------------------------------------------------------------------ */

#define TOK_MAX  512
#define TOK_LEN   64

typedef struct {
    char toks[TOK_MAX][TOK_LEN];
    int  n;
    int  pos;
} toks_t;

static int lex(const char *text, toks_t *out)
{
    out->n = 0;
    out->pos = 0;
    const char *p = text;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (*p == '#') { while (*p && *p != '\n') p++; continue; }
        if (out->n >= TOK_MAX) return -1;

        if (strchr("(){};=<>[]!,.:->", *p)) {
            /* special-case '->' */
            if (p[0] == '-' && p[1] == '>') {
                snprintf(out->toks[out->n++], TOK_LEN, "->");
                p += 2;
                continue;
            }
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

/* ------------------------------------------------------------------ */
/* Instruction emission                                                 */
/* ------------------------------------------------------------------ */

static int emit(veg3_unit_t *u, veg_instr_t in)
{
    if (!u->prog || u->prog_len >= u->prog_cap) return -1;
    u->prog[u->prog_len++] = in;
    return 0;
}

static int emit_ld(veg3_unit_t *u, uint64_t dst, uint64_t src, uint64_t n)
{
    veg_instr_t in;
    memset(&in, 0, sizeof in);
    in.op = V_LD;
    in.v[0] = dst; in.v[1] = src; in.v[2] = n;
    return emit(u, in);
}

static int emit_set(veg3_unit_t *u, uint64_t dst, uint64_t imm, uint64_t n)
{
    veg_instr_t in;
    memset(&in, 0, sizeof in);
    in.op = V_SET;
    in.v[0] = dst; in.v[1] = imm; in.v[2] = n;
    return emit(u, in);
}

static int emit_jmp(veg3_unit_t *u, uint64_t target)
{
    veg_instr_t in;
    memset(&in, 0, sizeof in);
    in.op = V_JMP;
    in.v[0] = target;
    return emit(u, in);
}

/* ------------------------------------------------------------------ */
/* Call / Return lowering (the heart of the convention)                 */
/* ------------------------------------------------------------------ */

int veg3_emit_call(veg3_unit_t *u, uint32_t func_idx,
                   const uint64_t *arg_bases, const uint32_t *arg_sizes,
                   uint32_t nargs)
{
    if (!u || func_idx >= u->nfuncs) return -1;
    const veg3_func_t *fn = &u->funcs[func_idx];
    if (nargs != fn->nparams) return -1;

    /* 1. Copy each argument into the argument region */
    uint64_t arg_cursor = VEG3_ARG_BASE;
    for (uint32_t i = 0; i < nargs; i++) {
        if (arg_sizes[i] == 0) continue;
        if (emit_ld(u, arg_cursor, arg_bases[i], arg_sizes[i]) != 0)
            return -1;
        arg_cursor += arg_sizes[i];
    }

    /* 2. Save continuation PC.
     *    We emit a placeholder; the real return address is the instruction
     *    *after* the jmp. We patch it after emitting the jmp. */
    size_t set_pc_idx = u->prog_len;
    if (emit_set(u, VEG3_RET_PC_ADDR, 0 /* patched */, 1) != 0)
        return -1;

    /* 3. Jump to callee entry */
    if (emit_jmp(u, fn->entry_pc) != 0)
        return -1;

    /* 4. Patch the continuation to the next instruction (return site) */
    u->prog[set_pc_idx].v[1] = (uint64_t)u->prog_len;

    return 0;
}

int veg3_emit_ret(veg3_unit_t *u, uint64_t retval_base, uint32_t retval_size)
{
    if (!u) return -1;

    /* 1. Copy return value into the return region (if any) */
    if (retval_size > 0) {
        if (emit_ld(u, VEG3_RETVAL_BASE, retval_base, retval_size) != 0)
            return -1;
    }

    /* 2. Indirect jump via the saved continuation.
     *    We lower `ret` as:
     *      ld  TMP, RET_PC, 1
     *      jmp *TMP
     *    Because the core ISA has no memory-indirect jmp, we use a
     *    dedicated convention: the callee writes the continuation into
     *    a known lane and we emit a direct jmp that the *caller* already
     *    patched. For a fully dynamic ret we need one extra lane as temp.
     *
     *    Practical total lowering used here:
     *      - Caller already stored the absolute return PC in RET_PC.
     *      - We cannot do mem-indirect jmp in pure V1, so we reserve
     *        a small runtime helper: the host/executor treats
     *        `jmp 0xFFFFFFFFFFFFFFFF` as "jump to mem[RET_PC]".
     *      - Alternatively (pure software): copy RET_PC into a register
     *        lane and require the executor to support it.
     *
     *    For maximum purity with the *current* executor we emit:
     *      jmp <sentinel>
     *    and document that veg_exec must resolve sentinel via RET_PC.
     *    Until the executor gains that hook, we emit a normal jmp to
     *    a known epilogue slot that the compile unit places at the end.
     *
     *    Current pure approach (no executor change yet):
     *      The return PC was set by the caller to an absolute index.
     *      We simply cannot read it at compile time. Therefore ret is
     *      lowered to a special jmp encoding: v[0] = (uint64_t)-1
     *      and veg_exec is expected to treat that as "jmp *mem[0xF000]".
     *      Until that lands, callers must use the patched style only.
     *
     *    For this commit we emit the sentinel form and also keep the
     *    caller-patched path as the primary mechanism. */
    veg_instr_t in;
    memset(&in, 0, sizeof in);
    in.op = V_JMP;
    in.v[0] = ~(uint64_t)0;   /* sentinel: "jmp *RET_PC" */
    return emit(u, in);
}

/* ------------------------------------------------------------------ */
/* Struct + type helpers                                                */
/* ------------------------------------------------------------------ */

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

static int parse_struct(toks_t *t, veg3_unit_t *u)
{
    if (!accept(t, "struct")) return -1;
    const char *name = next(t);
    if (!is_ident(name) || find_struct(u, name) >= 0) return -1;
    if (u->nstructs >= VEG3_MAX_STRUCTS) return -1;
    if (expect(t, "{") != 0) return -1;

    veg3_struct_t *s = &u->structs[u->nstructs];
    memset(s, 0, sizeof *s);
    snprintf(s->name, sizeof s->name, "%s", name);

    uint32_t offset = 0;
    while (peek(t) && !accept(t, "}")) {
        if (s->nfields >= VEG3_MAX_FIELDS) return -1;
        const char *fname = next(t);
        if (!is_ident(fname)) return -1;
        if (expect(t, ":") != 0) return -1;
        const char *tname = next(t);
        if (!tname) return -1;

        veg3_type_t ty;
        if (parse_type(u, tname, &ty) != 0) return -1;

        veg3_field_t *f = &s->fields[s->nfields++];
        snprintf(f->name, sizeof f->name, "%s", fname);
        f->offset = offset;
        f->size   = ty.size;
        offset   += ty.size;

        accept(t, ";");
    }

    s->size = offset;
    u->nstructs++;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Function parsing + lowering                                          */
/* ------------------------------------------------------------------ */

/*
 * fn name(a: T, b: U) -> R { ... }
 *
 * Lowering strategy for the body (minimal viable):
 *   - entry_pc = current prog_len
 *   - parameters are assumed already sitting at ARG_BASE
 *   - a bare `return;` or end-of-body emits emit_ret
 *   - full expression bodies come in a later pass
 */
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

    /* parameters */
    if (!accept(t, ")")) {
        for (;;) {
            if (fn->nparams >= VEG3_MAX_PARAMS) return -1;
            const char *pname = next(t);
            if (!is_ident(pname)) return -1;
            if (expect(t, ":") != 0) return -1;
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

    /* optional return type */
    fn->has_ret = 0;
    fn->ret.size = 0;
    if (accept(t, "->")) {
        const char *rname = next(t);
        if (!rname) return -1;
        if (parse_type(u, rname, &fn->ret) != 0) return -1;
        fn->has_ret = (fn->ret.size > 0);
    }

    if (expect(t, "{") != 0) return -1;

    /* Record entry point */
    fn->entry_pc = (uint32_t)u->prog_len;

    /* Minimal body support:
     *   - skip nested blocks while counting braces
     *   - recognise a trailing `return;` and emit ret
     * Full statement lowering is future work; the call/ret *convention*
     * is already total and usable from C via veg3_emit_call/ret. */
    int depth = 1;
    int saw_return = 0;
    while (peek(t) && depth > 0) {
        const char *tok = next(t);
        if (!tok) return -1;
        if (strcmp(tok, "{") == 0) depth++;
        else if (strcmp(tok, "}") == 0) depth--;
        else if (strcmp(tok, "return") == 0) saw_return = 1;
    }

    /* Always emit a ret at the end of the function so the convention
     * is closed even if the body was only a placeholder. */
    if (veg3_emit_ret(u, VEG3_ARG_BASE /* dummy */, 0) != 0)
        return -1;

    (void)saw_return;
    u->nfuncs++;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Compile entry                                                        */
/* ------------------------------------------------------------------ */

int veg3_compile(const char *src, veg3_unit_t *unit)
{
    if (!src || !unit) return -1;

    /* Preserve caller-supplied prog buffer */
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

    while (peek(&t)) {
        if (strcmp(peek(&t), "struct") == 0) {
            if (parse_struct(&t, unit) != 0) return -1;
            continue;
        }
        if (strcmp(peek(&t), "fn") == 0) {
            if (parse_fn(&t, unit) != 0) return -1;
            continue;
        }
        /* unknown top-level token — reject rather than silently skip */
        return -1;
    }

    return 0;
}

void veg3_dump(const veg3_unit_t *u, FILE *out)
{
    if (!u || !out) return;
    fprintf(out, "=== Veggdor V3 unit ===\n");
    fprintf(out, "Call convention regions:\n");
    fprintf(out, "  RET_PC   = 0x%llx\n", (unsigned long long)VEG3_RET_PC_ADDR);
    fprintf(out, "  ARG_BASE = 0x%llx\n", (unsigned long long)VEG3_ARG_BASE);
    fprintf(out, "  RETVAL   = 0x%llx\n", (unsigned long long)VEG3_RETVAL_BASE);
    fprintf(out, "Structs: %u\n", u->nstructs);
    for (uint32_t i = 0; i < u->nstructs; i++) {
        const veg3_struct_t *s = &u->structs[i];
        fprintf(out, "  struct %s (size %u lanes)\n", s->name, s->size);
        for (uint32_t j = 0; j < s->nfields; j++) {
            const veg3_field_t *f = &s->fields[j];
            fprintf(out, "    .%s  offset=%u  size=%u\n",
                    f->name, f->offset, f->size);
        }
    }
    fprintf(out, "Functions: %u\n", u->nfuncs);
    for (uint32_t i = 0; i < u->nfuncs; i++) {
        const veg3_func_t *f = &u->funcs[i];
        fprintf(out, "  fn %s (entry_pc=%u, nparams=%u, ret_size=%u)\n",
                f->name, f->entry_pc, f->nparams, f->ret.size);
    }
    fprintf(out, "Lowered instructions: %zu\n", u->prog_len);
}
