.data
## allocate memory space for Martix A, B, C data in shared data memory. (start from 0x8000)
mat_A:
.byte 1 4 5 3
.byte 1 4 2 2
.byte 5 5 3 3
.byte 4 3 1 5
mat_B:
.byte 1 4 3 4
.byte 5 5 5 2
.byte 4 4 5 4
.byte 3 4 3 5
mat_C:
.byte 0 0 0 0
.byte 0 0 0 0
.byte 0 0 0 0
.byte 0 0 0 0

## allocate memory space for constant value in shared data memory for computation.
ACCEL_REG_BASE_ADDR:
.word 0x12000

ACCEL_MEM_BASE_ADDR:
.word 0x20000

DMA_REG_BASE_ADDR:
.word 0xF000

ACCEL_OFFSET_ENABLE:
.word 0x0

ACCEL_OFFSET_STATUS:
.word 0x4

ACCEL_MATA_SIZE:
.word 0x8

ACCEL_MATB_SIZE:
.word 0xC

ACCEL_MATC_SIZE:
.word 0x10

ACCEL_OFFSET_MATA_MEM_ADDR:
.word 0x14

ACCEL_OFFSET_MATB_MEM_ADDR:
.word 0x18

ACCEL_OFFSET_MATC_MEM_ADDR:
.word 0x1c

ACCEL_OFFSET_MAT_MEM_STRIDE:
.word 0x20

DMA_OFFSET_ENABLE:
.word 0x0

DMA_OFFSET_SOURCE_INFO:
.word 0x4

DMA_OFFSET_DEST_INFO:
.word 0x8

DMA_OFFSET_SIZE_CFG:
.word 0xc

DMA_OFFSET_DONE:
.word 0x14

.text
######################
##                  ##
##      Step 0      ##
##                  ##
######################

## s0 -> base address of Matrix A in shared data memory.
la s0, mat_A
## s1 -> base address of Matrix B in shared data memory.
la s1, mat_B
## s2 -> base address of Matrix C in shared data memory.
la s2, mat_C

## s3 -> address of ACCEL_MEM_BASE_ADDR
la s3, ACCEL_MEM_BASE_ADDR
lw s3, 0(s3)

la t6, ACCEL_REG_BASE_ADDR
lw t0, 0(t6)

## 1. Program MATA_MEM_ADDR reg
## t1 -> 0x14 (Base address of MATA_MEM_ADDR reg)
la t6, ACCEL_OFFSET_MATA_MEM_ADDR
lw t1, 0(t6)
## t1 = 0x12000 + 0x14
add t1, t1, t0
## store 0x00 into mem[0x12014]
sw x0, 0(t1)

## 2. Program MATB_MEM_ADDR reg
## t1 -> 0x18 (Base address of MATB_MEM_ADDR reg)
la t6, ACCEL_OFFSET_MATB_MEM_ADDR
lw t1, 0(t6)
## t1 = 0x12000 + 0x18
add t1, t1, t0
## store 0x10 into mem[0x12018]
li t2, 0x10
sw t2, 0(t1)

## 3. Program MATC_MEM_ADDR reg
## t1 -> 0x1c (Base address of MATC_MEM_ADDR reg)
la t6, ACCEL_OFFSET_MATC_MEM_ADDR
lw t1, 0(t6)
## t1 = 0x12000 + 0x1C
add t1, t1, t0
## store 0x20 into mem[0x1201C]
li t2, 0x20
sw t2, 0(t1)

## 4. Program ACCEL_MATA_SIZE reg
## t1 -> 0x8 (Base address of ACCEL_MATA_SIZE reg)
la t6, ACCEL_MATA_SIZE
lw t1, 0(t6)
## t1 = 0x12000 + 0x8
add t1, t1, t0
li t2, 0x00030003
## store 0x00003003 into mem[0x12008]
sw t2, 0(t1)

## 5. Program ACCEL_MATB_SIZE reg
## t1 -> 0xC (Base address of ACCEL_MATB_SIZE reg)
la t6, ACCEL_MATB_SIZE
lw t1, 0(t6)
## t1 = 0x12000 + 0xC
add t1, t1, t0
## store 0x00030003 into mem[0x1200C]
li t2, 0x00030003
sw t2, 0(t1)

## 3. Program ACCEL_MATC_SIZE reg
## t1 -> 0x3003 (Base address of ACCEL_MATC_SIZE reg)
la t6, ACCEL_MATC_SIZE
lw t1, 0(t6)
## t1 = 0x12000 + 0x10
add t1, t1, t0
## store 0x00030003 into mem[0x12010]
li t2, 0x00030003
sw t2, 0(t1)

## 4. Program MAT_MEM_STRIDE reg
## t1 -> 0x20 (Base address of MAT_MEM_STRIDE reg)
la t6, ACCEL_OFFSET_MAT_MEM_STRIDE
lw t1, 0(t6)
## t1 = 0x12000 + 0x20
add t1, t1, t0
## store 0x00040404 into mem[0x12020]
li t2, 0x00040404
sw t2, 0(t1)


## 5. Enable accelerator
## t1 -> 0x00 (Base address of MAT_MEM_STRIDE reg)
la t6, DMA_OFFSET_ENABLE
lw t1, 0(t6)
## t1 = 0x12000 + 0x00
add t1, t1, t0
## store 0x00000001 into mem[0x12000]
li t2, 0x00000001
sw t2, 0(t1)

######################
##                  ##
##      Step 3      ##
##                  ##
######################
## ... executing matmul ...
## wait accelerator to finish computation
wait_sa:
## t1 -> 0x04 (offset of STATUS reg)
la t6, ACCEL_OFFSET_STATUS
lw t1, 0(t6)
## t1 = 0x12000 + 0x04
add t1, t1, t0
## load value of mem[0x12004]
lw t2, 0(t1)
beq t2, x0, wait_sa

######################
##                  ##
##      Step 4      ##
##                  ##
######################
## reset DONE reg in DMA
sw x0, 0(t1)
hcf