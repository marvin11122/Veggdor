# Effects — Capability without bloat

Human codebases drown in boilerplate: error handling ceremony, allocator dance,
framework glue, logging noise. Veggdor effects are the opposite:

> One effect value = one observable capability.  
> No hidden I/O. No ambient authority. No 40-line try/catch.

---

## Design rules (density)

1. **Effects are data** sitting in lanes, like everything else.
2. **The core never performs side effects silently.**  
   Either the host handles the effect, or it stays pending and visible to the tracer.
3. **Minimal vocabulary.** Only what is needed to bootstrap systems:

| Effect | Lanes (args) | Result |
|--------|----------------|--------|
| `Nop` | — | — |
| `Alloc` | `n` | base address or 0 |
| `Free` | `base, n` | unit |
| `Read` | `fd, dst, n` | bytes_read |
| `Write` | `fd, src, n` | bytes_written |
| `Open` | `path_base, path_len, mode` | fd or -1 |
| `Close` | `fd` | unit |
| `Exit` | `code` | halts |

That is enough to build files, sockets (fd-based), buffers, and process lifetime  
without importing an entire standard library.

---

## Encoding (total, inspectable)

Effect descriptor lives in a fixed region so the tracer always sees it:

```
0xFE00  effect_op     (enum)
0xFE01  arg0
0xFE02  arg1
0xFE03  arg2
0xFE04  result
0xFE05  status        (0=pending, 1=done, 2=error)
```

Program writes the descriptor, then executes the special instruction:

```
effect          ; host must handle; core only traps to host
```

Until the host runs, `status` stays `pending`.  
After the host runs, lanes contain the result.  
Every step remains visible.

---

## Why this beats human-style coding for AIs

- No exception hierarchy to invent
- No errno folklore
- No header soup
- One pattern for every capability: fill lanes → `effect` → read result
- The AI can verify the program by stepping; the host can be mocked in tests

Density > ceremony.

---

## Host contract

A host is any C (or other) runtime that:

1. Detects the `effect` instruction (or polls `0xFE05 == pending`)
2. Reads op + args
3. Performs the real syscall / allocation
4. Writes result + status
5. Resumes the machine

The Veggdor program never links against libc directly.  
Capability is granted only through this gate.

---

## Implementation status

| Piece | Status |
|-------|--------|
| Spec (this file) | Done |
| Lane encoding | Done |
| `V_EFFECT` opcode + executor hook | Next (code) |
| Minimal host (`veg_host.c`) | Next |
| Surface syntax `effect Alloc(n)` | After host |
