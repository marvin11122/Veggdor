/-
  Veggdor.lean — formal semantics of the V1 core ISA plus the
  V2 control layer (set, cmp, jz, jmp) and the V3 call
  convention (continuation PC at 0xF000).

  Memory  : Nat → UInt64   (flat lane array)
  Vector  : (base : Nat) × (len : Nat)
  Arithmetic : UInt64 (wraps mod 2^64 by definition)
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

/- ------------------------------------------------------------ -/
/- V2 control: set, cmp, jz, jmp                               -/
/- ------------------------------------------------------------ -/

/-- set dst imm n : dst[i] ← imm. -/
def opSet (m : Mem) (dst : Nat) (imm : UInt64) (n : Nat) : Mem :=
  m.storeVec dst (List.replicate n imm)

inductive CmpOp where
  | eq | ne | lt | le | gt | ge
  deriving DecidableEq, Repr

def cmpOk (a b : UInt64) : CmpOp → Bool
  | CmpOp.eq => a == b
  | CmpOp.ne => a != b
  | CmpOp.lt => a < b
  | CmpOp.le => a <= b
  | CmpOp.gt => a > b
  | CmpOp.ge => a >= b

/-- cmp f a b op : f ← 1 if (m.load a) op (m.load b) else 0. -/
def opCmp (m : Mem) (f a b : Nat) (op : CmpOp) : Mem :=
  m.store f (if cmpOk (m.load a) (m.load b) op then (1 : UInt64) else (0 : UInt64))

/-- jz flag target : returns target if mem[flag] == 0, none otherwise. -/
def opJz (m : Mem) (flag : Nat) (target : Nat) : Option Nat :=
  if m.load flag == 0 then some target else none

/-- jmp target : pc ← target (V3 return: `jmp *mem[0xF000]`). -/
def opJmp (_m : Mem) (target : Nat) : Nat := target

/-- V3 call convention: the caller writes the continuation (return) PC
    into 0xF000 before `jmp`ing to the callee entry. -/
def V3_RET_PC : Nat := 0xF000

/-- ret: jmp back to the stored continuation. -/
def opRet (m : Mem) : UInt64 := m.load V3_RET_PC

end Veggdor

#check Veggdor.exec
#check Veggdor.opLd_zero
#check Veggdor.opAdd_zero
#check Veggdor.opSet
#check Veggdor.opCmp
#check Veggdor.opJz
#check Veggdor.opRet
