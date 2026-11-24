# Effects — Capability without bloat

> One effect value = one observable capability.  
> No hidden I/O. No ambient authority. No 40-line try/catch.

---

## Vocabulary

| Effect | Args | Result |
|--------|------|--------|
| `Nop` | — | — |
| `Alloc` | `n` | base lane |
| `Free` | `base, n` | unit |
| `Read` | `fd, dst, n` | bytes_read |
| `Write` | `fd, src, n` | bytes_written |
| `Open` | `path_base, path_len, mode` | fd_idx |
| `Close` | `fd` | unit |
| `Exit` | `code` | halts |

`mode`: 0=ro, 1=rw, 2=create/trunc

---

## Encoding

```
0xFE00  effect_op
0xFE01  arg0
0xFE02  arg1
0xFE03  arg2
0xFE04  result
0xFE05  status   (0=pending 1=done 2=error)
```

Program fills lanes, executes `effect`, host runs, result is in lanes.  
Tracer sees everything.

---

## Status

| Piece | State |
|-------|--------|
| Spec | Done |
| `V_EFFECT` opcode | Live |
| `veg_exec_host` / `veg_step_host` | Live |
| Alloc / Free / Exit | Live |
| Open / Close / Read / Write | **Live** (fd table, POSIX) |
| Surface syntax sugar | Open |

---

## Density for AIs

```
set EFFECT_OP, OPEN
set ARG0, path_base
set ARG1, path_len
set ARG2, 2
effect
; RESULT = fd_idx
```

No libc headers in the program. No errno folklore.  
Capability is granted only through this gate.
