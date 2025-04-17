.text
start:
    # -------------------------------
    # Step 1: Initialize Data Memory
    # -------------------------------
    li  x10, 0x2000      
    # Start address (0x2000)
    li  x11, 0x2fff
    # End address (0x2FFF)

init_memory:
    slli x12, x10, 1     
    # x12 = x10 * 2
    sw   x12, 0(x10)     
    # Store value at address x10
    addi x10, x10, 4     
    # Increment address by 4 (word size)
    bge  x11, x10, init_memory 
    # Loop until 0x2FFF


dma_phase_1:
    li  x1, 0x0000F000   
    # DMA MMIO Base Address
    li  x2, 0x2000       
    # SOURCE_INFO (0x2000)
    li  x3, 0x3000       
    # DEST_INFO (0x3000)
    li  x4, 0x40403f3f
    # 64 * 64 = 4096
    # DMA_SIZE_CFG (4KB)
    li  x5, 1            
    # ENABLE (Start DMA)
    
    # Program DMA
    sw  x2, 4(x1)
    sw  x3, 8(x1)
    sw  x4, 12(x1)
    sb  x5, 0(x1)


check_complete1:
    # Check whether the DMA command completes
    lw  x6, 20(x1)
    and x6, x6, x5
    beq x6, x0, check_complete1
    sw  x0, 20(x1)

dma_phase_1:
    li  x1, 0x0000F000   
    # DMA MMIO Base Address
    li  x2, 0x2e00       
    # SOURCE_INFO (0x2e00)
    li  x3, 0x4000       
    # DEST_INFO (0x4000)
    li  x4, 0x20201f3f
    # 32 * 64 = 2048  / width = stride = 32 
    # DMA_SIZE_CFG (2KB)
    li  x5, 1            
    # ENABLE (Start DMA)
    # Program DMA
    sw  x2, 4(x1)
    sw  x3, 8(x1)
    sw  x4, 12(x1)
    sb  x5, 0(x1)

check_complete2:
    # Check whether the DMA command completes
    lw  x6, 20(x1)
    and x6, x6, x5
    beq x6, x0, check_complete2
    sw  x0, 20(x1)

exit:
    hcf