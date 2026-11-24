#include "veg3.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Tiny lexer (shared style with veg2)                                  */
/* ------------------------------------------------------------------ */

#define TOK_MAX  256
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

        if (strchr("(){};=<>[]!,.:", *p)) {
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
               !strchr("(){};=<>[]!,.:", *p) && k < TOK_LEN - 1)
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
    if (!s || !*s || !isalpha((unsigned char)*s) && *s != '_') return 0;
    for (const char *c = s + 1; *c; c++)
        if (!isalnum((unsigned char)*c) && *c != '_') return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Struct registration                                                  */
/* ------------------------------------------------------------------ */

static int find_struct(const veg3_unit_t *u, const char *name)
{
    for (uint32_t i = 0; i < u->nstructs; i++)
        if (strcmp(u->structs[i].name, name) == 0) return (int)i;
    return -1;
}

static int parse_type_size(veg3_unit_t *u, const char *tname, uint32_t *size,
                           int *is_struct, int *struct_idx)
{
    *is_struct = 0;
    *struct_idx = -1;
    if (strcmp(tname, "v1") == 0) { *size = 1; return 0; }
    if (strcmp(tname, "v8") == 0) { *size = 8; return 0; }
    int idx = find_struct(u, tname);
    if (idx >= 0) {
        *size = u->structs[idx].size;
        *is_struct = 1;
        *struct_idx = idx;
        return 0;
    }
    return -1;
}

/* struct Name { field: Type; ... } */
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

        uint32_t fsize = 0;
        int is_s = 0, sidx = -1;
        if (parse_type_size(u, tname, &fsize, &is_s, &sidx) != 0) return -1;

        veg3_field_t *f = &s->fields[s->nfields++];
        snprintf(f->name, sizeof f->name, "%s", fname);
        f->offset = offset;
        f->size   = fsize;
        offset   += fsize;

        accept(t, ";");  /* optional semicolon */
    }

    s->size = offset;
    u->nstructs++;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Compile entry (skeleton — grows with function lowering)              */
/* ------------------------------------------------------------------ */

int veg3_compile(const char *src, veg3_unit_t *unit)
{
    if (!src || !unit) return -1;
    memset(unit, 0, sizeof *unit);
    unit->next_addr = 0x100;

    toks_t t;
    if (lex(src, &t) != 0) return -1;

    /* First pass: collect structs (and later function signatures) */
    while (peek(&t)) {
        if (strcmp(peek(&t), "struct") == 0) {
            if (parse_struct(&t, unit) != 0) return -1;
            continue;
        }
        /* For now skip function bodies / other statements —
           they will be lowered in subsequent commits. */
        next(&t);
    }

    return 0;
}

void veg3_dump(const veg3_unit_t *u, FILE *out)
{
    if (!u || !out) return;
    fprintf(out, "=== Veggdor V3 unit ===\n");
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
        fprintf(out, "  fn %s (entry_pc=%u, frame=%u)\n",
                f->name, f->entry_pc, f->frame_size);
    }
    fprintf(out, "Lowered instructions: %zu\n", u->prog_len);
}
