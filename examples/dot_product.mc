# Dot product: [1, 2, 3, 4] · [5, 6, 7, 8] = 70
# Lanes are uint64; vectors occupy four consecutive lanes.

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
