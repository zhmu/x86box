cpu 8086
org 100h

section .text
    mov     ax,cs
    mov     ds,ax
    mov     es,ax

    ; dos: get irq 0
    push    es
    mov     ax,3500h
    int     21h
    mov     [old_de+0],bx
    mov     [old_de+2],es
    pop     es

    ; dos: set irq 0
    mov     ax,2500h
    mov     dx,division_error
    int     21h

    mov     ah,3ch
    xor     cx,cx
    mov     dx,fname
    int     21h
    jc      die
    mov     [handle],ax

next2:
    mov     di,result
next:
    ; set flags
    mov     ax,2h
    cmp     byte [cf],0
    jz      cf_off
    or      ax,1            ; CF
cf_off:
    push    ax
    popf

    ; set up registers
    mov     al,0ffh
    mov     [intn],al
    mov     ax,[val1w]
    mov     bl,[val2]

    ; do operation
    aas

    ; store results (al / flags / error)
    stosw
    pushf
    pop     ax
    stosw
    ;mov     al,[intn]
    ;stosb

    mov     ah,40h
    mov     bx,[handle]
    mov     dx,result
    mov     cx,di
    sub     cx,dx
    int     21h

    ;inc     byte [val2]
    ;jnz     next

    inc     word [val1w]
    jnz     next2

    inc     byte [cf]
    cmp     byte [cf],1
    jne     next2

    ; close file
    mov     ah,3eh
    mov     bx,[handle]
    int     21h

    jmp     bye

die:
    mov     dx,errormsg
    mov     ah,9
    int     21h

bye:
    ; restore irq 0
    mov     dx,[old_de+0]
    mov     ax,[old_de+2]
    mov     ds,ax
    mov     ax,2500h
    int     21h

    int     20h

write_buffer:
    ; dos: write data
    mov     ah,40h
    mov     bx,[handle]
    mov     dx,result
    mov     cx,di
    sub     cx,dx
    int     21h

    mov     ax,0e24h ; $
    int     10h
    ret

division_error:
    push    bp
    mov     bp,sp
    add     word ss:[bp+2],2 ; skip div
    mov     byte cs:[intn],00h

    push    ax
    mov     ax,0e24h
    int     10h
    pop     ax
    pop     bp
    iret

val1        db  0
val2        db  0
intn        db  0
val1w       dw  0
val2w       dw  0
handle      dw  0
prev_flags  dw  0
old_de      dd  0
cf          db  0

fname       db  "out.bin",0
errormsg    db  "error$"
result      db  0
