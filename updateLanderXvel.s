@ updateLanderXvel.s

.global updateLanderXvel
updateLanderXvel:
    ldr r3, [r0]
    cmp r1, #1
    beq .right
@ move left
    sub r3, r3, #30
    b .return
@ move right
.right:
    add r3, r3, #30
.return:
    str r3, [r0]
@ decrement fuel
    ldr r3, [r2]
    sub r3, r3, #1
    str r3, [r2]
@done
    mov pc, lr
