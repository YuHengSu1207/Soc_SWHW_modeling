.data
src_data: .word 0x01, 0x02, 0x03, 0x04, 0x11, 0x12, 0x13, 0x14
          .word 0x05, 0x06, 0x07, 0x08, 0x15, 0x16, 0x17, 0x18

dest_data: .word 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
           .word 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

.text
start:
    li  x1, 0x0000f000 
    # DMA_MMIO_BASE_ADDR
    
    la  x2, src_data   
    # SOURCE_INFO
    la  x3, dest_data  
    # DEST_INFO
    li  x4, 0x20180f01 
    # DMA_SIZE_CFG
    
    li  x5, 1

    # Program DMA
    sw  x2, 4(x1)
    sw  x3, 8(x1)
    sw  x4, 12(x1)
    sb  x5, 0(x1)

check_complete:
    # Check whether the DMA command completes
    lw  x6, 20(x1)
    and x6, x6, x5
    beq x6, x0, check_complete
    sw  x0, 20(x1)

read_results:
    lw  x16, 0(x3)
    lw  x17, 4(x3)
    lw  x18, 8(x3)
    lw  x19, 12(x3)
    lw  x20, 16(x3)
    lw  x21, 20(x3)
    
    lw  x24, 24(x3)
    lw  x25, 28(x3)
    lw  x26, 32(x3)
    lw  x27, 36(x3)
    lw  x28, 40(x3)
    lw  x29, 44(x3)

exit:
    hcf