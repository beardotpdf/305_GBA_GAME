@ getIndex.s

.global getIndex
getIndex:
    @ modulo 256
    and r0, r0, #255
    and r1, r1, #255

    @ determine row and column
    mov r0, r0, asr #3
    mov r1, r1, asr #3

    @ determine tile index
    mov r2, #32
    mul r1, r2, r1

    add r0, r0, r1

    mov pc, lr
