# Veggdor Vision — The AI-Native Systems Language

## The Bet

Most languages were designed for humans who already understand machines.  
Veggdor is designed for **machines that must not be allowed to guess**.

The core constraint remains absolute:

> Execution is understanding.  
> If you cannot step it, you do not understand it.

Everything above the six vector ops is just syntax sugar that **must** lower to a fully observable, deterministic machine state. No escape hatches. No "runtime magic". No probabilistic semantics.

---

## Why this can become extreme

An AI that generates Veggdor does not need to "hope" the code works.  
It can:

1. Emit the program
2. Step / trace it
3. Observe every lane
4. Prove local properties (via the Lean model)
5. Iterate with perfect feedback

That feedback loop is what current languages refuse to give.  
Veggdor makes the feedback loop the entire point.

---

## Language layers (the stack)

```
V3  —  Human/AI surface          (functions, structs, modules, effects)
 ↓
V2  —  Structured control        (while, if, variables)     [exists]
 ↓
V1  —  Pure vector ISA           (ld/st/add/sub/mul/red)    [exists + formalized]
 ↓
Mem —  Flat 64-bit lanes         (total, wrap, alias-legal)
```

Every higher construct **must** have a total lowering.  
If it cannot be lowered, it is illegal.

---

## V3 design principles (the part that makes AIs dangerous)

1. **One memory model**  
   Everything is lanes. Structs are contiguous lanes. Arrays are base+stride.  
   No hidden pointers. No object headers. No GC.

2. **Total functions**  
   Every function has explicit input/output vector regions.  
   No ambient state except the flat memory the caller can already see.

3. **Effects are data**  
   I/O, allocation, and foreign calls are explicit effect values that the  
   machine can step over or refuse. Nothing happens "behind" the program.

4. **Regular syntax, zero ambiguity**  
   The grammar is small and closed. An AI that has seen 50 examples  
   can generate correct programs without creative interpretation.

5. **Observability is mandatory**  
   `trace`, `step`, and `disasm` are not debugging tools.  
   They are part of the language contract.

---

## What "build Firefox in 10 minutes" actually requires

Not magic. A stack of total lowerings:

| Capability | Status | Path |
|------------|--------|------|
| Vector arithmetic + reduce | Done | V1 |
| Structured control | Done | V2 |
| Formal core semantics | Done | Lean |
| Functions + call convention | Done | V3 |
| Structs / arrays of vectors | Done | V3 |
| Explicit effect system (I/O, alloc) | Done | V3 |
| Self-hosting compiler | Later | V3 → V1 |
| Browser-scale libraries | Later | on top of effects |

The AI does not "write Firefox".  
It writes a series of total, steppable programs that together implement the necessary subsystems — each one fully inspectable.

That is the only honest version of the claim.

---

## Immediate next construction targets

1. **V3 function + call convention**  
   Explicit argument/return regions in lane space.  
   No stack magic — just conventions on the flat memory.

2. **Structs as lane layouts**  
   `struct Point { x: v1, y: v1 }` → two consecutive lanes.  
   Layout is part of the type and visible to the tracer.

3. **Effect values**  
   `effect Read : (addr, len) → bytes`  
   `effect Write : (addr, len, bytes) → ()`  
   The machine either performs them or records them.  
   Never both silently.

4. **AI generation contract**  
   A short, machine-readable document that tells any LLM:  
   "Here is exactly how to emit correct Veggdor. Here is how to verify it."

---

## Non-goals (the things that would destroy the thesis)

- Hidden garbage collection
- Implicit allocations
- Undefined behavior
- "Fast path" that is not observable
- Any construct whose lowering cannot be shown

---

## The standard of success

A language is "the most extreme" not when it has the most features,  
but when an AI can be given a hard systems task and the **only**  
feedback it needs is the step tracer and the formal model.

Veggdor is being built to that standard.
