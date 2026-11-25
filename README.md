free TAY K

# Veggdor

**An AI-oriented programming language built around explicit machine state.**

Veggdor’s core is a small vector machine: programs operate on a flat array of
64-bit lanes using explicit instructions. The goal is not to claim that an AI
cannot make mistakes; it is to make program behavior concrete, inspectable,
and testable instead of relying on an informal interpretation of generated
code.

## Start here

Requirements: a C11 compiler and `make`. Lean 4 is optional and is needed only
to check the current Lean model.

```sh
make
make test
make mc
```

`make mc` builds and runs `example.mc` through `veg_mc`, the standalone
machine-code runner. To run another machine-code file:

```sh
./veg_mc path/to/program.mc
./veg_mc --trace path/to/program.mc
```

The runner prints the parsed instructions, executes them in zero-initialized
memory, and reports nonzero lanes. Machine programs use lane addresses and
the instruction forms documented below. `--trace` reports each executed
instruction and the memory-lane changes observed since the preceding step.

Runnable example: `make demo-dot` executes
[`examples/dot_product.mc`](examples/dot_product.mc), which computes the
dot product `[1, 2, 3, 4] · [5, 6, 7, 8]`; the result is written to lane
`0x130` (`70`, or `0x46`).

## Machine model

- Memory is a flat array of `uint64_t` lanes.
- Vector addresses and lengths are measured in lanes.
- Arithmetic wraps modulo 2⁶⁴.
- Memory accesses are bounds-checked by the executor.
- `ld` and `st` copy lanes; overlapping copies are handled by the executor.
- Machine execution has no implicit I/O. Host capabilities are explicit
  `effect` instructions handled by a host callback.

## V1 instruction set

| Instruction | Form | Effect |
|---|---|---|
| `ld` | `ld dst, src, n` | Copy `n` lanes from `src` to `dst`. |
| `st` | `st dst, src, n` | Copy `n` lanes from `src` to `dst`. |
| `add` | `add dst, a, b, n` | Elementwise unsigned 64-bit addition. |
| `sub` | `sub dst, a, b, n` | Elementwise subtraction. |
| `mul` | `mul dst, a, b, n` | Elementwise multiplication. |
| `red` | `red dst, src, n, op` | Reduce `n` lanes with `add`, `max`, or `min`. |
| `set` | `set dst, value, n` | Fill `n` lanes with `value`. |
| `cmp` | `cmp flag, a, b, op` | Compare scalar lanes; write `0` or `1` to `flag`. |
| `jz` | `jz flag, target` | Jump when the flag lane is zero. |
| `jmp` | `jmp target` | Jump to an instruction index. |
| `effect` | `effect` | Transfer control to the configured host callback. |

The executor also exposes single-step, trace, and host-enabled execution APIs
in `veg.h`. See `EFFECTS.md` for the effect descriptor and host contract.

## Language layers and implementation status

| Layer | What exists | Entry points |
|---|---|---|
| V1 | Machine instructions, parser, executor, disassembler, step/trace APIs | `veg.h`, `veg.c`, `veg_mc.c` |
| V2 | Structured frontend with declarations, assignments, conditionals, loops, and constant initialization | `veg2.h`, `veg2.c` |
| V3 | Implemented subset for functions, fixed-layout structs, field access, and call lowering | `veg3.h`, `veg3.c`, `V3_SPEC.md` |
| Host effects | POSIX-backed allocation, file, and exit operations through an explicit callback | `veg_host.h`, `veg_host.c`, `EFFECTS.md` |
| Lean | Partial formal model; it is not yet a proof of equivalence with the C executor | `lean/Veggdor.lean` |

“Implemented” means the corresponding code and tests exist; it does not mean
the language is feature-complete, self-hosting, verified end-to-end, or ready
to replace a general-purpose operating system. The host implementation is
POSIX-specific. The V3 specification may describe more than the current
compiler supports.

## AI generation contract

When generating a Veggdor program, use only constructs implemented by the
selected frontend. State the lane layout, identify reads and writes, and run
the tests or execution trace. Do not infer semantics from syntax alone.

- [AI_CONTRACT.md](AI_CONTRACT.md) — current generation rules
- [V3_SPEC.md](V3_SPEC.md) — V3 design and lowering conventions
- [EFFECTS.md](EFFECTS.md) — explicit host effects
- [VISION.md](VISION.md) — project direction

## Development

```sh
make clean
make
make test
lean lean/Veggdor.lean   # optional; checks the current Lean file
```

Tests are local executable checks, not a claim of exhaustive correctness.
Contributions should include a regression test for each behavior change and
keep the documented machine semantics aligned with the executor.
