free TAY K

<div align="center">

# VEGGDOR

### THE MACHINE IS THE LANGUAGE.

**AI-native programming, down to explicit machine state.**

No hidden runtime. No semantic hand-waving.<br>
Write the operations. Execute them. Inspect every lane.

[Machine model](#the-machine) · [Run it](#run-it) · [AI contract](AI_CONTRACT.md) · [Status](#what-exists-today)

</div>

---

## The thesis

Language models generate tokens. Veggdor does not pretend otherwise.
The bet is different: make the program they generate **small, explicit,
executable, and inspectable**—so correctness is tested against actual state
transitions instead of trusted because an explanation sounds convincing.

> **Generation may be probabilistic. Execution is not.**

Veggdor starts at the machine, not at a pile of abstractions:
instructions operate on a flat memory of 64-bit lanes. Higher-level
constructs are useful only when their lowering is visible and testable.

## The uncomfortable part

**AI can produce code faster than most teams can explain what it guarantees.**
The industry is scaling output; mathematical confidence is not scaling with it.

“The model said it works” is not an invariant.<br>
“The tests passed” is not a proof.<br>
“It looks right” is not a state transition.

If you cannot name the invariant, show the state, and reproduce the failure,
you do not know what the program guarantees. That is not an insult to
programmers; it is a bar the whole AI-coding industry has to clear.

Veggdor's challenge: **less code theater, more executable math.**

## The economic bias

The market does not automatically reward truth. It rewards what can be sold,
scaled, measured, and locked in. In AI software, that can favor more output
and more usage over fewer claims that are actually verified.

That is an incentive problem—not proof of a conspiracy, and not a claim that
every company or engineer behaves the same way. But when verification slows
the demo and exposes uncertainty, the business pressure is obvious:
**ship the answer; postpone proving it.**

Veggdor takes the opposite side:

- execution over persuasion;
- inspectable state over black-box confidence;
- explicit limits over inflated capability claims;
- reproducible evidence over vendor promises.

The point is not to pretend software can escape economics. It is to make
correctness harder to fake and easier to inspect—whoever owns the tool.

## Run it

Requirements: C11 compiler + `make`. Lean 4 is optional.

```sh
make
make test
make demo-dot
```

The demo computes `[1, 2, 3, 4] · [5, 6, 7, 8] = 70` in Veggdor machine
code. Run any `.mc` program directly:

```sh
./veg_mc examples/dot_product.mc
./veg_mc --trace examples/dot_product.mc
```

The trace prints each executed instruction and the lane changes it causes.
The result is in lane `0x130`:

```text
mem[0x0130] = 0x46
```

## The machine

Memory is a flat array of `uint64_t` lanes. Addresses and lengths are lane
indices. Arithmetic wraps modulo 2⁶⁴; memory access is bounds-checked.

| Instruction | Form | Meaning |
|---|---|---|
| `ld` | `ld dst, src, n` | Copy `n` lanes |
| `st` | `st dst, src, n` | Copy `n` lanes |
| `add` | `add dst, a, b, n` | Elementwise addition |
| `sub` | `sub dst, a, b, n` | Elementwise subtraction |
| `mul` | `mul dst, a, b, n` | Elementwise multiplication |
| `red` | `red dst, src, n, op` | Reduce with `add`, `max`, or `min` |
| `set` | `set dst, value, n` | Fill lanes with a constant |
| `cmp` | `cmp flag, a, b, op` | Write scalar comparison as `0` or `1` |
| `jz` | `jz flag, target` | Jump if the flag lane is zero |
| `jmp` | `jmp target` | Jump to an instruction index |
| `effect` | `effect` | Explicit handoff to a host callback |

Aliasing is allowed. Overlapping copies have defined behavior. The core
executor performs no implicit I/O.

## Example: machine code, not a mockup

[`examples/dot_product.mc`](examples/dot_product.mc):

```text
# Dot product: [1, 2, 3, 4] · [5, 6, 7, 8] = 70
set 0x100, 1, 1
set 0x101, 2, 1
set 0x102, 3, 1
set 0x103, 4, 1
set 0x110, 5, 1
set 0x111, 6, 1
set 0x112, 7, 1
set 0x113, 8, 1
mul 0x120, 0x100, 0x110, 4
red 0x130, 0x120, 4, add
```

The machine performs four lane-wise multiplications, then reduces the
products: `5 + 12 + 21 + 32 = 70`. `--trace` exposes the execution state;
the final memory dump is not inferred from the source.

## The stack

```text
V3  functions · structs · effects       implemented subset
 ↓
V2  variables · assignments · control  implemented frontend
 ↓
V1  vector machine instructions         executable core
 ↓
MEM flat 64-bit lanes                   explicit state
```

| Component | Reality today |
|---|---|
| V1 machine, executor, disassembler, step/trace | Implemented and tested |
| `veg_mc` standalone machine-code runner | Implemented; strict parse errors |
| V2 structured frontend | Implemented subset; see `veg2.h` |
| V3 functions, fixed-layout structs, lowering | Implemented subset; see `V3_SPEC.md` |
| Host effects | Explicit callback; current host is POSIX-specific |
| Lean model | Partial model; **not** a proof of C-executor equivalence |
| Browser / general-purpose OS / self-hosting | Not implemented |

“Implemented” means code and tests exist—not feature completeness,
production certification, or a proof of correctness. Read
[`V3_SPEC.md`](V3_SPEC.md) alongside the compiler: the spec can exceed the
currently supported subset.

## Built for AI inspection

An AI-generated program should be answerable in machine terms:

1. Which instruction executes next?
2. Which lanes does it read and write?
3. What exact state changes?
4. Can the result be replayed and tested?

Veggdor’s contract is to make those questions concrete. It does not remove
the need for good specifications, tests, threat models, or human judgment.

Start with [`AI_CONTRACT.md`](AI_CONTRACT.md). See also
[`EFFECTS.md`](EFFECTS.md), [`VISION.md`](VISION.md), and
[`lean/Veggdor.lean`](lean/Veggdor.lean).

## Build and verify

```sh
make clean
make
make test
lean lean/Veggdor.lean   # optional
```

Tests are executable checks, not exhaustive verification. Contributions
should include regression tests and keep documented semantics aligned with
the implementation.
