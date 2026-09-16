bits 32

global user_start
global user_end

user_start:
    mov eax, 0x1337
    int 0x80

    mov eax, 1
    int 0x80

user_end: