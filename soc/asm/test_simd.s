.text
# ==== saddi8i8s.vv ====
li x10, 0x01020304       
# rs1 = [01, 02, 03, 04]
li x11, 0x05060708      
# rs2 = [05, 06, 07, 08]
saddi8i8s.vv x12, x10, x11
# Expected: [06, 08, 0a, 0c]

# ==== saddi16i16s.vv ====
li x13, 0x12345678       
# rs1 = [1234, 5678]
li x14, 0x11111111       
# rs2 = [1111, 1111]
saddi16i16s.vv x15, x13, x14
# Expected: [2345, 6789]

# ==== ssubi8i8s.vv ====
li x16, 0x05060708       
# rs1 = [05, 06, 07, 08]
li x17, 0x01020304       
# rs2 = [01, 02, 03, 04]
ssubi8i8s.vv x18, x16, x17
# Expected: [04, 04, 04, 04]

# ==== ssubi16i16s.vv ====
li x19, 0x56781234       
# rs1 = [5678, 1234]
li x20, 0x11111111       
# rs2 = [1111, 1111]
ssubi16i16s.vv x21, x19, x20
# Expected: [4567, 0123]

# ==== spmuli8i16s.vv.l ====
li x22, 0x01020304       
# rs1 = [01, 02, 03, 04]
li x23, 0x02020202       
# rs2 = [02, 02, 02, 02]
spmuli8i16s.vv.l x24, x22, x23
# Expected low 2 results as 16-bit values: 
# [0006, 0008] → x24

# ==== spmuli8i16s.vv.h ====
spmuli8i16s.vv.h x25, x22, x23
# Expected upper 2 results: [0002, 0004] → x25

# ==== samuli8i8s.vv.nq ====
li x26, 0x0F0E0D0C       
# rs1 = [0F, 0E, 0D, 0C]
li x27, 0x01020304       
# rs2 = [01, 02, 03, 04]
samuli8i8s.vv.nq x28, x26, x27
# Multiply → take MSB byte of each product → x28
# [00,00,00,00] x25
# ==== samuli8i8s.vv.l ====
samuli8i8s.vv.l x29, x26, x27
# Multiply → take LSB byte of each product → x29
# [0f 1c 27 30]
hcf