/-
  Veggdor.lean — minimal formal semantics of the V1 core ISA

  Memory  : Nat → UInt64   (flat lane array)
  Vector  : (base : Nat) × (len : Nat)
  Arithmetic : UInt64 (wraps mod 2^64 by definition)

  Only the six data-parallel operations are formalised here.
  Control (cmp/jz/jmp) is left for a later layer.
-/

namespace Veggdor

/-- Flat memory: address → 64-bit lane. -/
def Mem := Nat → UInt64

/-- Empty memory (all zeros). -/
def Mem.empty : Mem := fun _ => 0

/-- Write a single lane. -/
def Mem.store (m : Mem) (addr : Nat) (val : UInt64) : Mem :=
  fun a => if a == addr then val else m a

/-- Read a single lane. -/
def Mem.load (m : Mem) (addr : Nat) : UInt64 := m addr

/-- Write a contiguous vector. -/
def Mem.storeVec (m : Mem) (dst : Nat) (v : List UInt64) : Mem :=
  match v with
  | []      => m
  | x :: xs => Mem.storeVec (m.store dst x) (dst + 1) xs

/-- Read a contiguous vector of length `len`. -/
def Mem.loadVec (m : Mem) (src : Nat) (len : Nat) : List UInt64 :=
  match len with
  | 0     => []
  | n + 1 => m.load src :: Mem.loadVec m (src + 1) n

/- ------------------------------------------------------------ -/
/- Core operations                                                -/
/- ------------------------------------------------------------ -/

/-- ld dst src n  :  dst[i] ← src[i] -/
def opLd (m : Mem) (dst src n : Nat) : Mem :=
  m.storeVec dst (m.loadVec src n)

/-- add dst a b n  :  dst[i] ← a[i] + b[i]  (mod 2^64) -/
def opAdd (m : Mem) (dst a b n : Nat) : Mem :=
  let va := m.loadVec a n
  let vb := m.loadVec b n
  m.storeVec dst (List.zipWith (· + ·) va vb)

/-- sub dst a b n  :  dst[i] ← a[i] - b[i]  (mod 2^64) -/
def opSub (m : Mem) (dst a b n : Nat) : Mem :=
  let va := m.loadVec a n
  let vb := m.loadVec b n
  m.storeVec dst (List.zipWith (· - ·) va vb)

/-- mul dst a b n  :  dst[i] ← a[i] * b[i]  (mod 2^64) -/
def opMul (m : Mem) (dst a b n : Nat) : Mem :=
  let va := m.loadVec a n
  let vb := m.loadVec b n
  m.storeVec dst (List.zipWith (· * ·) va vb)

/-- Reduction operators. -/
inductive RedOp where
  | add | max | min
  deriving DecidableEq, Repr

def redAdd : List UInt64 → UInt64
  | []      => 0
  | x :: xs => xs.foldl (· + ·) x

def redMax : List UInt64 → UInt64
  | []      => 0
  | x :: xs => xs.foldl (fun a b => if a < b then b else a) x

def redMin : List UInt64 → UInt64
  | []      => 0
  | x :: xs => xs.foldl (fun a b => if a < b then a else b) x

def applyRed : RedOp → List UInt64 → UInt64
  | .add, v => redAdd v
  | .max, v => redMax v
  | .min, v => redMin v

/-- red dst a n op  :  dst[0] ← reduce(a[0..n), op) -/
def opRed (m : Mem) (dst a n : Nat) (op : RedOp) : Mem :=
  m.store dst (applyRed op (m.loadVec a n))

/- ------------------------------------------------------------ -/
/- Instruction language                                           -/
/- ------------------------------------------------------------ -/

inductive Instr where
  | ld  (dst src n : Nat)
  | st  (dst src n : Nat)          -- same as ld in this model (pure memory)
  | add (dst a b n : Nat)
  | sub (dst a b n : Nat)
  | mul (dst a b n : Nat)
  | red (dst a n : Nat) (op : RedOp)
  deriving Repr

/-- Big-step semantics of one instruction. -/
def exec1 (m : Mem) : Instr → Mem
  | .ld  dst src n   => opLd  m dst src n
  | .st  dst src n   => opLd  m dst src n   -- store is ld with swapped roles in surface syntax
  | .add dst a b n   => opAdd m dst a b n
  | .sub dst a b n   => opSub m dst a b n
  | .mul dst a b n   => opMul m dst a b n
  | .red dst a n op  => opRed m dst a n op

/-- Execute a whole program (ordered list). -/
def exec (m : Mem) (prog : List Instr) : Mem :=
  prog.foldl exec1 m

/- ------------------------------------------------------------ -/
/- Basic theorems                                                 -/
/- ------------------------------------------------------------ -/

/-- Loading an empty vector changes nothing. -/
theorem loadVec_zero (m : Mem) (src : Nat) :
    m.loadVec src 0 = [] := rfl

/-- Storing an empty vector is the identity. -/
theorem storeVec_nil (m : Mem) (dst : Nat) :
    m.storeVec dst [] = m := rfl

/-- ld of length 0 is identity on memory. -/
theorem opLd_zero (m : Mem) (dst src : Nat) :
    opLd m dst src 0 = m := by
  simp [opLd, Mem.storeVec, Mem.loadVec]

/-- add of length 0 is identity on memory. -/
theorem opAdd_zero (m : Mem) (dst a b : Nat) :
    opAdd m dst a b 0 = m := by
  simp [opAdd, Mem.storeVec, Mem.loadVec]

end Veggdor

#check Veggdor.exec
#check Veggdor.opLd_zero
#check Veggdor.opAdd_zero
