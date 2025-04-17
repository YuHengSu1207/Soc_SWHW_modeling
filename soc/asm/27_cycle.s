.data
data_area: .word 0, 0, 0

.text
# Load the base address of the .data section into x1
la x1, data_area

li x2, 0x12345678
li x3, 0xabcdef01

# Store test values
sw x2, 0(x1)   
# Store word x2 at data_area + 0
sh x3, 4(x1)   
# Store halfword x3[15:0] at data_area + 4
sb x2, 6(x1)   
# Store byte x2[7:0] at data_area + 6
sb x3, 8(x1)   
# Store byte x3[7:0] at data_area + 8

# Load and verify stored values
lw x5, 0(x1)   
# Load word from data_area + 0 into x5
lh x6, 4(x1)   
# Load halfword from data_area + 4 into x6
lb x7, 6(x1)   
# Load byte from data_area + 6 into x7
lb x8, 8(x1)   
# Load byte from data_area + 8 into x8

hcf