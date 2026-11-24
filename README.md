# Veggdor

**A minimal vector machine designed so that execution is understanding.**

Veggdor is being built as an AI-native systems language:  
a substrate where an AI can generate, step, observe, and verify programs  
without ever appealing to statistical guesswork.

> Every operation must be directly executable, fully observable, and free of predictive ambiguity.

No hidden control flow in the data-parallel core.  
No opaque runtime.  
No probabilistic interpretation layer.  
No allocation, no exceptions, no I/O inside the model unless made explicit.

If a system cannot step through a Veggdor program and know exactly what happens next, the problem is in the system — not in the language.

---

## The stack

```
V3   Human/AI surface          (functions, structs, effects)   ← in design
 ↓
V2   Structured control        (while, if, variables)          [exists]
 ↓
V1   Pure vector ISA           (ld/st/add/sub/mul/red)         [exists + Lean]
 ↓
Mem  Flat 64-bit lanes         (total, wrap, alias-legal)
```

Every higher construct must have a **total lowering**.  
If it cannot be lowered, it is illegal.

See [VISION.md](VISION.md) and [AI_CONTRACT.md](AI_CONTRACT.md).

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

## V2 — Structured surface (current legal AI surface)

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

Lowers to `cmp` / `jz` / `jmp` + the core ops + `set`.

---

## Full observability

| API | Purpose |
|-----|---------|
| `veg_disasm` | Instruction → text |
| `veg_step` | Execute exactly one instruction |
| `veg_exec_trace` | Callback before every instruction with full state |
| `veg_exec` | Bulk deterministic execution |

A tracer sees the complete machine state before every step.  
There is nothing left to guess.

---

## Formal semantics (Lean 4)

`lean/Veggdor.lean` formalises the six core operations with no dependencies.

```bash
lean lean/Veggdor.lean
```

---

## For AIs

Read [AI_CONTRACT.md](AI_CONTRACT.md) before generating any program.  
It defines the only legal surface, the verification obligations, and what you must refuse.

---

## Status

| Component | State |
|-----------|--------|
| V1 parser + executor | Complete |
| Disassembler + step + trace | Complete |
| V2 structured frontend | Working |
| Lean 4 core semantics | Present |
| V3 (functions, structs, effects) | In design (see VISION.md) |
| AI generation contract | Present |

---

## Build

```bash
make
make test
```

---

## The standard

A language is extreme not when it has the most features,  
but when an AI can be given a hard systems task and the only feedback  
it needs is the step tracer and the formal model.

That is the standard Veggdor is being built to.
