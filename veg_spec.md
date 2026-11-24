# Veggdor V1 — brute force-Vektor-Sprache (from scratch)

## Grundmodell

- Speicher: Flachspeicher aus 64-Bit-Lanes (Two's Complement). Adresse = Ganzzahl, Lane `a+i`.
- Vektor = (Adresse, Länge `n`). Operanden sind Adressen.

## ISA — sechs Instruktionen, kein Kontrollfluss

Syntax: `op dst, src, n` … alle Operanden Adressen, `n` immediates Länge (Lane-Zählung).

| Mnem | Operanden | Semantik |
|---|---|---|
| `ld`  | `dst, src, n` | `dst[i] := src[i]` |
| `st`  | `dst, src, n` | `src[i] -> dst[i]` |
| `add` | `dst, a, b, n` | `dst[i] := a[i] + b[i]` |
| `sub` | `dst, a, b, n` | `dst[i] := a[i] - b[i]` |
| `mul` | `dst, a, b, n` | `dst[i] := a[i] * b[i]` |
| `red` | `dst, a, n, op` | `dst[0] := Reduce(a[0..n))` |

`red`-op ∈ {`add`, `max`, `min`}.

## Brutalität

- Kein Kontrollfluss: Instruktionen laufen in geordneter Reihenfolge.
- Aliasing erlaubt (`dst == a`).
- Overflow wrap mod 2^64.
- Kein I/O, keine Mallok, keine Ausnahmen: Programm = Instruktionen + Daten.

## Beispiel (`a = 0x100`, out = `0x200`)

```
ld  0x200, 0x100, 8
add 0x300, 0x100, 0x200, 8
mul 0x200, 0x100, 0x300, 8
red 0x400, 0x200, 8, add
```

# V2 — Das neue C (structured)

Design-Lineage: functional/lisp-flavored, systems/C-flavored, scripting/python-flavored, proof-oriented/lean4-flavored — realisiert als V1/V2 Maschinensprache.

## Sprache (C-Dialekt)

Statements:
- `v8 name;` — Variable deklarieren (Adresse wird beim Parsen vergeben).
- `x = y;` — Zuweisung → `ld x, y, 8`
- `x = y op z;` mit op ∈ {+, -, *} → `add/sub/mul x, y, z, 8`
- `if (a OP b) stmt` / `while (a OP b) stmt`, OP ∈ {<, <=, >, >=, ==, !=}

## Generierte Maschinenbefehle

- `cmp f, a, b, op` — f[0] = (a[0] op b[0]) ? 1 : 0
- `jz target, f` — springe zu `target`, wenn f[0] == 0
- `jmp target`

## Beispiel (Summe 1..8 in `s`)

```
v8 s;
v8 i;
v8 n;
v8 t;
v8 one;
while (i < n) {
  t = i + one;
  i = t;
  s = s + i;
}
```

### Konstanten-Initialisierung (V2b)

- `v8 a = 42;` → `set a, 42, 8` (Broadcast auf alle 8 Lanes)
- `v8 b = [1, 2, 3, 4, 5, 6, 7, 8];` → 8 × `set b+k, c_k, 1` (Vector-Literal)
- Neues Maschinenbefehl: `set dst, imm, n`
