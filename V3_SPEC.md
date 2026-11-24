# V3 Specification — Functions, Structs, Effects

**Status:** Design frozen for implementation.  
**Rule:** Every construct below must have a total, mechanical lowering to V1/V2.  
If it cannot be lowered, it is illegal.

---

## 1. Memory regions (the only storage)

```
[0x0000 .. 0x00FF]   reserved / flags
[0x0100 .. ...]      user variables & structs   (V2 already starts here)
[0xF000 .. 0xF0FF]   call frame metadata
[0xF100 .. ...]      argument / return regions (convention)
```

Everything is still a flat `uint64_t` lane array.  
No hidden pointers. No object headers. No GC.

---

## 2. Structs = fixed lane layouts

```
struct Point {
  x : v1;     // 1 lane
  y : v1;     // 1 lane
}             // total size = 2 lanes

struct Particle {
  pos  : Point;   // 2 lanes
  vel  : Point;   // 2 lanes
  mass : v1;      // 1 lane
}                 // total size = 5 lanes
```

Lowering:
- `Point p;`  → allocate 2 consecutive lanes, record base address
- `p.x`       → base + 0
- `p.y`       → base + 1
- `Particle q; q.pos.x` → base + 0

Layout is part of the type and visible to the tracer.

---

## 3. Functions — explicit argument / return regions

```
fn add_points(a: Point, b: Point) -> Point {
  out.x = a.x + b.x;
  out.y = a.y + b.y;
  return out;
}
```

### Call convention (total, no magic)

1. Caller writes arguments into a known argument region (e.g. `0xF100`).
2. Caller writes return-address (continuation PC) into frame metadata.
3. `call target` = `jmp target` + bookkeeping.
4. Callee reads args from the argument region.
5. Callee writes result into the return region.
6. Callee executes `ret` = `jmp` back to the saved continuation.

There is no hardware stack.  
The "stack" is just lanes the program itself manages under a convention.

### Lowering sketch

```
// call add_points(p1, p2)
ld  0xF100, p1_base, 2      // copy arg a
ld  0xF102, p2_base, 2      // copy arg b
set 0xF000, <return_pc>, 1  // save continuation
jmp <add_points>

// at return site:
ld  result_base, 0xF200, 2  // copy return value
```

Every step remains visible to `veg_step` / `veg_exec_trace`.

---

## 4. Effects — data, not magic

```
effect Read  (addr: v1, len: v1) -> bytes
effect Write (addr: v1, len: v1, data: bytes) -> unit
effect Alloc (n: v1) -> addr          // still just a lane range grant
```

An effect is a value that the machine either:
- performs (and records the result into lanes), or
- leaves pending for an outer host.

The core executor never performs side effects silently.  
If the program contains an effect, the tracer sees it.

---

## 5. Modules (later, still total)

```
mod physics {
  struct Particle { ... }
  fn integrate(p: Particle, dt: v1) -> Particle { ... }
}
```

A module is a namespace + a collection of functions/structs.  
Linking = concatenating instruction streams + fixing up addresses.  
Still fully inspectable.

---

## 6. What remains illegal

- Implicit allocation
- Hidden stacks or frames the tracer cannot see
- Undefined behaviour
- Any construct whose lowering cannot be shown as a finite sequence of V1/V2 instructions + explicit effect values

---

## 7. Implementation order

1. Struct layout + field access in the V2/V3 surface  (next code)
2. Function definition + call/ret convention
3. Effect values + host interface
4. Update `AI_CONTRACT.md` so AIs may emit the new surface
5. Lean model extension for call frames

---

## Success metric for V3

An AI can emit a multi-function program with structs,  
the tracer can single-step every lane access and every call/return,  
and the Lean model can talk about the same operations.

No guessing. No runtime magic. Only total lowerings.
