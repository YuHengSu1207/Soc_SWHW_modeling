.data
data:
.byte 1 2 3 4 5 6 7 8

.text
la      t0, data
lh      t1, 0(t0)
addi    t2, x0, 9
sb      t2, 2(t0)
lw      t3, 0(t0)
hcf