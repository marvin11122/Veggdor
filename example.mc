# Machine program (Veggdor V1 ISA, standalone): out = 5 + 7 in 4 lanes, sum = 48
set 0x100, 5, 4
set 0x108, 7, 4
add 0x110, 0x100, 0x108, 4
red 0x118, 0x110, 4, add
