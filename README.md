# Veggdor

**A minimal vector machine designed so that execution is understanding.**

Veggdor is a bare-metal vector ISA and a thin structured language compiled down to it.  
It was built from scratch with one constraint: every operation must be directly executable, observable, and free of predictive ambiguity. No hidden control flow in the core, no opaque runtime, no probabilistic interpretation layer.

The point is not performance. The point is legibility under execution.

---

## Design thesis

Most languages and intermediate representations that AIs (and many humans) interact with are optimized for *prediction* — pattern completion over large corpora. Veggdor is optimized for the opposite:

- Flat 64-bit lane memory
- Explicit vector length on every instruction
- Six core operations + a minimal control layer
- Deterministic wrap-around arithmetic
- No allocation, no exceptions, no I/O in the core model

If a system cannot step through a Veggdor program and know exactly what happens next, the problem is in the system, not in the language.

---

## V1 — Core ISA (no control flow)

Memory is a flat array of 64-bit two’s-complement lanes.  
A vector is simply `(address, length)`.

| Mnemonic | Form | Meaning |
|----------|------|---------|
| `ld`  | `dst, src, n` | `dst[i] ← src[i]` |
| `st`  | `dst, src, n` | `src[i] → dst[i]` |
| `add` | `dst, a, b, n` | `dst[i] ← a[i] + b[i]` |
| `sub` | `dst, a, b, n` | `dst[i] ← a[i] − b[i]` |
| `mul` | `dst, a, b, n` | `dst[i] ← a[i] * b[i]` |
| `red` | `dst, a, n, op` | `dst[0] ← reduce(a[0..n), op)` |

`red` supports `add`, `max`, `min`.  
Aliasing is allowed. Overflow wraps mod 2⁶⁴.  
Programs are ordered lists of instructions; there is no branching in V1.

---

## V2 — Structured layer

A small C-/Lisp-flavored surface that lowers to the core plus three control instructions:

- `cmp f, a, b, op`
- `jz  target, f`
- `jmp target`
- `set dst, imm, n` (broadcast / literal)

Example (sum 1..n):

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

---

## Status

| Component | State |
|-----------|--------|
| V1 parser + executor | Working |
| V2 structured frontend | Working |
| Tests | Present |
| Spec | `veg_spec.md` |
| Formalization | Not yet |

This is still a research prototype. The interesting property is not that it is finished; it is that the execution model is small enough to be fully inspected and, if desired, fully formalized.

---

## Why this exists

Most systems that claim to “understand” code do so by statistical approximation.  
Veggdor is an attempt to build a substrate where understanding collapses to stepping.  
If that turns out to be useful for verification, for AI interpretability, or for teaching what a machine actually does, good.  
If not, it remains a clean, tiny vector machine that does exactly what it says.

---

## Build

```bash
make
```

See `example.v`, `veg_test.c`, `veg2_test.c`, and `veg_spec.md`.
