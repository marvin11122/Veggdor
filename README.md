# Veggdor

**A minimal vector machine designed so that execution is understanding.**

Veggdor is a bare-metal vector ISA and a thin structured language that lowers to it.  
It was built from scratch under one non-negotiable constraint:

> Every operation must be directly executable, fully observable, and free of predictive ambiguity.

No hidden control flow in the data-parallel core.  
No opaque runtime.  
No probabilistic interpretation layer.  
No allocation, no exceptions, no I/O inside the model.

If a system cannot step through a Veggdor program and know exactly what happens next, the problem is in the system — not in the language.

---

## Design thesis

Most languages and intermediate representations that AIs (and many humans) interact with are optimized for *prediction* — pattern completion over large corpora. Veggdor is optimized for the opposite:

| Property | Choice |
|----------|--------|
| Memory | Flat 64-bit lane array |
| Vector | Explicit `(address, length)` on every instruction |
| Arithmetic | Deterministic wrap mod 2⁶⁴ |
| Core ops | Six data-parallel instructions |
| Control | Minimal, fully explicit (`cmp` / `jz` / `jmp`) |
| Observability | Disassembler + single-step + trace callback |

Understanding collapses to stepping.

---

## V1 — Core ISA

| Mnemonic | Form | Meaning |
|----------|------|---------|
| `ld`  | `dst, src, n` | `dst[i] ← src[i]` |
| `st`  | `dst, src, n` | `src[i] → dst[i]` |
| `add` | `dst, a, b, n` | `dst[i] ← a[i] + b[i]` |
| `sub` | `dst, a, b, n` | `dst[i] ← a[i] − b[i]` |
| `mul` | `dst, a, b, n` | `dst[i] ← a[i] * b[i]` |
| `red` | `dst, a, n, op` | `dst[0] ← reduce(a[0..n), op)` |

`red` supports `add`, `max`, `min`. Aliasing is legal. Overlap is handled correctly.

---

## V2 — Structured surface

A small C-/Lisp-flavoured syntax that lowers onto the core plus:

- `cmp f, a, b, op`
- `jz  flag, target`
- `jmp target`
- `set dst, imm, n`

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

## Full observability (the part that matters)

| API | Purpose |
|-----|---------|
| `veg_disasm` | Turn any instruction back into readable text |
| `veg_step` | Execute exactly one instruction, update PC |
| `veg_exec_trace` | Run with a callback before every instruction |
| `veg_exec` | Bulk execution (still fully deterministic) |

A tracer sees the complete machine state before every step.  
There is nothing left to guess.

---

## Status

| Component | State |
|-----------|--------|
| V1 parser + executor | Complete |
| Disassembler | Complete |
| Single-step + trace | Complete |
| V2 structured frontend | Working |
| Tests | Present |
| Spec | `veg_spec.md` |
| Formal semantics | Open (the model is small enough) |

---

## Why this exists

Most systems that claim to “understand” code do so by statistical approximation.  
Veggdor is a substrate where understanding reduces to stepping through a tiny, total, deterministic machine.

If that turns out to be useful for verification, for AI interpretability research, or for teaching what a machine actually does — good.  
If not, it remains a clean vector machine that does exactly what it says and nothing else.

---

## Build

```bash
make
make test
```

See `example.v`, `veg_test.c`, `veg2_test.c`, and `veg_spec.md`.
