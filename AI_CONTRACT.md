# AI Contract for Veggdor

This document is the only interface an AI is allowed to use when generating or modifying Veggdor programs.

If you are an AI reading this: follow it exactly. Do not invent semantics. Do not assume runtime behaviour that is not listed here.

---

## 1. Hard rules

1. Every program must lower to the V1/V2 instruction set defined in `veg.h` and `veg_spec.md`.
2. Memory is a flat array of `uint64_t` lanes. There is no other storage.
3. All arithmetic is mod 2⁶⁴. Overflow is wrap-around. There is no UB.
4. Aliasing is legal. Overlapping `ld`/`st` must behave as defined in the C executor.
5. If you cannot express an idea with the current surface + total lowering, you must say so. You are not allowed to invent new instructions.

---

## 2. Current surface you may emit (V2)

```
v8 name;                       // declare 8-lane variable
v8 name = imm;                 // declare + broadcast immediate
v8 name = [v0,v1,...,v7];      // declare + lane literals
name = name;                   // copy
name = name op name;           // op ∈ {+, -, *}
if (a OP b) stmt_or_block      // OP ∈ {==,!=,<,<=,>,>=}
while (a OP b) stmt_or_block
```

Blocks use `{` `}`.

That is the entire legal surface right now.

---

## 3. How you verify your own output

After emitting a program you must be able to answer:

1. What is the exact sequence of V1/V2 instructions it lowers to?
2. For every instruction, what lanes are read and what lanes are written?
3. Can `veg_step` / `veg_exec_trace` run it without fault on a memory of size `VEG_MEM_SIZE`?

If you cannot answer these three questions, the program is not finished.

---

## 4. Generation procedure (mandatory)

When asked to solve a problem in Veggdor:

1. State the lane layout you will use (which variables live where).
2. Write the V2 source.
3. Mentally (or actually) lower it to the instruction list.
4. Check bounds: every address + length must stay inside the declared memory.
5. Only then present the program.

Never emit a program whose memory effects you have not accounted for.

---

## 5. What you must refuse

- Any request that requires hidden allocation
- Any request that requires undefined behaviour
- Any request that cannot be lowered to the current ISA
- Any "just trust the runtime" construct

Reply with the exact missing capability instead of inventing one.

---

## 6. Future surface (not yet legal)

These will be added only when a total lowering exists:

- Named functions with explicit argument/return regions
- Structs as fixed lane layouts
- Explicit effect values (I/O, external calls)

Until they appear in this contract with a lowering rule, they are illegal.

---

## 7. Success condition for the AI

You succeed when:

- The program runs under `veg_exec` / `veg_exec_trace`
- Every step is observable
- The Lean model can talk about the same operations
- No appeal to "what the language probably means" was required

That is the entire contract.
