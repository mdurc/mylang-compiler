; --- Embedded Runtime Library (X86_64) ---
	section .text

; architecture-specific syscall numbers
%ifdef TARGET_MACOS
  %define SYS_READ   0x2000003
  %define SYS_WRITE  0x2000004
  %define SYS_OPEN   0x2000005
  %define SYS_CLOSE  0x2000006
  %define SYS_EXIT   0x2000001
  %define SYS_MMAP   0x20000C5
  %define SYS_MUNMAP 0x2000049
  %define SYS_LSEEK  0x20000C7
  %define MAP_FLAGS  0x1002 ; MAP_PRIVATE | MAP_ANON
%elifdef TARGET_LINUX
  %define SYS_READ   0
  %define SYS_WRITE  1
  %define SYS_OPEN   2
  %define SYS_CLOSE  3
  %define SYS_EXIT   60
  %define SYS_MMAP   9
  %define SYS_MUNMAP 11
  %define SYS_LSEEK  8
  %define MAP_FLAGS  0x22 ; MAP_PRIVATE | MAP_ANONYMOUS
%endif

; architecture-specific macro to check syscall success
%macro EXEC_SYSCALL 1
  mov rax, %1
  syscall
%ifdef TARGET_MACOS
  jnc %%success ; macOS carry bit is set if an error occurred
  neg rax       ; macOS errno -> negate to match linux negative errno
%%success:
%endif
%endmacro

; rdi <- exit code
; exits program with provided `rdi` value
exit:
%ifdef TRACK_MEMORY
  push rdi
  call _check_memory_leaks
  pop rdi
%endif
  EXEC_SYSCALL SYS_EXIT
%ifdef TRACK_MEMORY
_check_memory_leaks:
  mov rdi, 2 ; output all prints to stderr
  lea rsi, [rel leak_msg_start]
  call print_string
  mov rdi, 2
  lea rsi, [rel leak_msg_usage]
  call print_string
  mov rdi, 2
  mov rsi, [rel _mem_alloc_count]
  call print_uint
  mov rdi, 2
  lea rsi, [rel leak_msg_allocs]
  call print_string
  mov rdi, 2
  mov rsi, [rel _mem_free_count]
  call print_uint
  mov rdi, 2
  lea rsi, [rel leak_msg_frees]
  call print_string
  mov rdi, 2
  lea rsi, [rel leak_msg_inuse]
  call print_string
  mov rdi, 2
  mov rsi, [rel _mem_active_bytes]
  call print_uint
  mov rdi, 2
  lea rsi, [rel leak_msg_bytes]
  call print_string
  ret
%endif

; rdi <- address of string
; rax -> length of string
string_length:
	xor rax, rax	; initialize to zero
.loop:
	cmp byte[rdi + rax], 0
	jz .end			; jump if we were on the null-byte

	inc rax
	jmp .loop
.end:
	ret

; rdi <- file descriptor (1 stdout, 2 stderr)
; rsi <- address of null-terminated string
print_string:
  push rdi          ; save fd
  push rsi          ; save addr
  mov rdi, rsi      ; prep arg for strlen
	call string_length
	mov rdx, rax      ; store length
  pop rsi
  pop rdi

  EXEC_SYSCALL SYS_WRITE
	ret

; rdi <- file descriptor
; rsi <- char code
print_char:
	push rsi	    ; push it to the stack so we can get the address
	mov rsi, rsp	; start at this address, treating it as a string
  mov rdx, 1    ; strlen of 1

  ; write the char that is now a string
  EXEC_SYSCALL SYS_WRITE
	add rsp, 8	; restore the address
	ret

; rdi <- file descriptor
; prints newline (0x0A) to fd
print_newline:
	push 0x0A	    ; get rsi to point to an address with 0x0A content.
	mov rsi, rsp	; alternative to having a newline buffer in .data.
	mov rdx, 1    ; 1 byte for char

  EXEC_SYSCALL SYS_WRITE
	add rsp, 8	; undo the 0x0A push
	ret

; clears the screen via ANSI escape codes
clrscr:
  push rdi
  push rsi
  push rdx
  push rax
  mov rdi, 1        ; fd hardcoded for stdout
  mov rsi, clr_scr  ; load address to rsi
  mov rdx, 7
  EXEC_SYSCALL SYS_WRITE
  pop rax
  pop rdx
  pop rsi
  pop rdi
  ret

; rdi <- file descriptor
; rsi <- unsigned 64-bit integer
; prints in decimal format to fd
print_uint:
	; converts integer into a null-term string, passed to print_string
	sub rsp, 32	; the max number of digits of uint64_t is ~20
	mov r8, rsp
	add r8, 31	    ; get to the highest byte, last char (little endian)
	mov byte[r8], 0 ; plant the null terminator
  mov rax, rsi
; rax <- value to convert
; r8 <- current position (starting at null-terminator)
.convert:
  ; rax is the dividend low part
	xor rdx, rdx	; dividend high part
	mov rcx, 10	  ; divisor
	div rcx		    ; [rax]/10, quotient := rax, remainder := rdx
	add dl, 0x30	; add '0' char to remainder byte for ascii value
	dec r8        ; decrease to the next valid space in the stack buffer
	mov [r8], dl
	test rax, rax
	jnz .convert

  ; fd arg is still in rdi
	mov rsi, r8   ; setup arg, pointer to start of string
	call print_string
	add rsp, 32   ; restore the stack
	ret

; rdi <- file descriptor
; rsi <- signed 64-bit integer
; prints in decimal format to fd
print_int:
	; converts integer into a null-term string, passed to print_string
	sub rsp, 32       ; arbitrary large buffer size
	mov r8, rsp
	add r8, 31        ; get to the highest byte, last char (little endian)
	mov byte[r8], 0   ; plant the null terminator

  mov rax, rsi      ; value to convert

	xor r9, r9        ; set sign flag to zero
	cmp rsi, 0
	jge .convert

	neg rax   ; make positive for conversion
	mov r9, 1	; set sign flag to one
; rax <- value to convert
; r8 <- current position (starting at null-terminator)
; r9 <- sign flag, 0=positive, 1=negative
.convert:
  ; rax is the dividend low part
	xor rdx, rdx	; dividend high part
	mov rcx, 10	  ; divisor
	div rcx		    ; [rax]/10, quotient := rax, remainder := rdx
	add dl, 0x30	; add '0' char to remainder byte for ascii value
	dec r8        ; decrease to the next valid space in the stack buffer
	mov [r8], dl
	test rax, rax
  jnz .convert

	; add '-' if the number was negative
	test r9, r9
	jz .done
	dec r8
	mov byte[r8], 0x2D ; '-' char
.done:
  ; rdi already has fd
	mov rsi, r8
	call print_string
	add rsp, 32	; restore the stack
	ret

; al -> character read from stdin, else 0 if the end of input stream occurs
read_char:
	mov rdi, 0	; stdin
	sub rsp, 8	; make room for 8 bytes on stack
	mov rsi, rsp	; ptr to new space on the stack
	mov rdx, 1	; only want to read 1 byte
	EXEC_SYSCALL SYS_READ
	test rax, rax
	jz .eof
	mov al, byte[rsp]
	jmp .done
.eof:
	xor rax, rax	; sets al to zero
.done:
	add rsp, 8	; restore context
	ret

; rdi <- address of buffer
; rsi <- size of buffer
; rax -> buffer address, else 0 if word doesn't fit the buffer
; reads next word from stdin (skipping whitespace) into the provided buffer.
; the returned buffer will be null-terminated.
read_word:
	xor rdx, rdx	; clear the index counter
.skip_ws:
	; skip any leading whitespace
	call .call_read_char
	test al, al
	jz .eof
	cmp al, 0x20	; space
	je .skip_ws
	cmp al, 0x09	; tab
	je .skip_ws
	cmp al, 0x0A	; newline
	je .skip_ws
.loop:
	cmp rdx, rsi-1	; account for the space needed for the null byte
	jae .err
	mov [rdi + rdx], al	; store the valid character that was last read
	inc rdx

	; break the loop on any trailing whitespace
	call .call_read_char
	test al, al
	jz .terminate
	cmp al, 0x20	; space
	je .terminate
	cmp al, 0x09	; tab
	je .terminate
	cmp al, 0x0A	; newline
	je .terminate
	jmp .loop	; else, we loop and store the value at the top of loop
.eof:
	test rdx, rdx
	jz .err		; if rdx is not zero, then we should fall-through
.terminate:
	; null-terminate
	mov byte[rdx + rdi], 0
	mov rax, rdi
	ret
.call_read_char:
	push rdi
	push rsi
	push rdx
	call read_char
	pop rdx
	pop rsi
	pop rdi
	ret
.err:
	xor rax, rax	; zero on overflow
	ret

; rdi <- address of null-terminated string
; rax -> parsed unsigned number
; rdx -> character count
parse_uint:
	xor rax, rax
	xor rdx, rdx
.loop:
	xor rcx, rcx	; clear prior to storage
	mov cl, byte[rdi + rdx]
	test cl, cl
	jz .done
	cmp cl, 0x30	; '0' char
	jb .done	; done if cl is below 0
	cmp cl, 0x39	; '9' char
	ja .done	; done if cl is above 9
	sub cl, 0x30	; get number from ascii (- '0')
	imul rax, 10	; rax := rax * 10
	add rax, rcx
	inc rdx
	jmp .loop
.done:
	ret

; rdi <- address of null-terminated string
; rax -> parsed signed number
; rdx -> character count (including sign if any)
; no space between sign and digits are allowed.
parse_int:
	xor r8, r8	; sign flag, 0 = positive, 1 = negative
	xor rsi, rsi	; start of unsigned portion
	mov cl, byte[rdi]
	cmp cl, 0x2D	; '-' char
	je .is_negative
	cmp cl, 0x2B	; '+' char
	je .is_positive
	jmp .parse
.is_negative:
	mov r8, 1
.is_positive:
	mov rsi, 1
	inc rdi		; move past the sign
.parse:
	call parse_uint	; sets rax (num) and rdx (size)
	add rdx, rsi	; add possible sign character to the length
	test r8, r8	; check sign
	jz .done
	neg rax
.done:
	ret

; rdi <- address of string
; rsi <- address of string
; rax -> 1 if they are equal, else 0
string_equals:
	call string_length
	mov rcx, rax	; save first len in rcx
	mov rdx, rdi	; temp save of first string
	mov rdi, rsi	; first = second
	mov rsi, rdx	; second = first (temp)
	call string_length
	cmp rcx, rax	; compare first len with second len (in rax)
	jne .not_equal
.loop:
	mov al, byte[rdi]
	mov cl, byte[rsi]
	cmp al, cl
	jne .not_equal

	test al, al	; check if we reached a null byte
	jz .equal

	inc rdi
	inc rsi
	jmp .loop
.equal:
	mov rax, 1
	ret
.not_equal:
	xor rax, rax
	ret

; rdi <- addr of null-terminated string
; rax -> address of heap-allocated copy, or 0 on failure
; copies the string from rdi into a newly allocated buffer on the heap
string_copy:
  push rdi      ; save src buffer

  call string_length
  inc rax   ; include null terminator in length
  push rax      ; save src length

  mov rdi, rax  ; size to allocate
  call malloc
  test rax, rax
  jz .err       ; if malloc failed, return 0

  mov rdi, rax  ; rdi is the dst buffer from malloc
  pop rcx       ; get src length
  pop rsi       ; get src buffer
  call memcpy

  ; rax is still dst buffer from malloc
  ret
.err:
	xor rax, rax
	ret

; rdi <- addr of first null-terminated str
; rsi <- addr of second null-terminated str
; rax -> addr of new null-terminated str (heap-allocated; caller must free)
string_concat:
  push rbx
  push rcx
  push rdx
  push r12
  push r13
  push r14
  push r15

  mov r12, rdi        ; save first string pointer
  mov r13, rsi        ; save second string pointer

  mov rdi, r12        ; arg1: first string
  call string_length
  mov r14, rax        ; r10 = len1

  mov rdi, r13        ; arg1: second string
  call string_length
  mov r15, rax       ; r11 = len2

  mov rax, r14
  add rax, r15
  add rax, 1         ; total length = len1 + len2 + 1 (for null)
  mov rdi, rax       ; arg1: malloc size
  call malloc
  test rax, rax
  jz .err            ; malloc failed
  mov rbx, rax       ; rbx = dest buffer

  ; copy first string
  mov rdi, rbx       ; dest
  mov rsi, r12       ; src
  mov rcx, r14       ; size of string 1
  call memcpy

  ; copy second string
  lea rdi, [rbx + r14] ; dest = buffer + len1
  mov rsi, r13         ; src = second string
  mov rcx, r15         ; size of string 2
  call memcpy

  ; null-terminate
  lea rax, [r14 + r15]
  mov byte [rbx + rax], 0
  mov rax, rbx         ; return pointer
  jmp .epilogue
  .err:
  xor rax, rax
  .epilogue:
  pop r15
  pop r14
  pop r13
  pop r12
  pop rdx
  pop rcx
  pop rbx
  ret

; rdi <- dst
; rsi <- src
; rdx <- size
memcpy:
  mov rax, rdi
  mov rcx, rdx
  cld           ; clear direction flag
  rep movsb     ; copy rcx bytes (this clobbers rdi, rsi, and rcx)
  ret           ; rax holds original dst

; rdi <- dst
; rsi <- value
; rdx <- size
memset:
	mov r8, rdi
	mov rax, rsi
	mov rcx, rdx
	cld           ; clear direction flag
	rep stosb     ; fill memory (clobbers rdi and rcx)
	mov rax, r8   ; restore original dst
	ret

%define SIGNATURE 0xDEADC0DEDEADC0DE
; rdi <- size in bytes
; rax -> allocated memory address (16-byte aligned), or 0 on error
malloc:
  test rdi, rdi
  jz .invalid_size
  add rdi, 16        ; total_size = requested_size + 16
  jc .error
  mov rsi, rdi       ; length = total_size
  xor rdi, rdi       ; addr = NULL (let kernel choose)
  mov rdx, 3         ; prot = PROT_READ | PROT_WRITE
  mov r10, MAP_FLAGS ; flags = MAP_ANON | MAP_PRIVATE
  mov r8, -1         ; fd = -1
  xor r9, r9         ; offset = 0
  EXEC_SYSCALL SYS_MMAP
  cmp rax, -1
  jle .error
  mov r8, SIGNATURE
  mov QWORD[rax], r8      ; store signature for identification in 'free'
  mov QWORD[rax + 8], rsi ; store total_size at the start of the block
%ifdef TRACK_MEMORY
  inc QWORD[rel _mem_alloc_count]
  add QWORD[rel _mem_active_bytes], rsi
%endif
  add rax, 16             ; return address after header
  ret
  .invalid_size:
  xor rax, rax
  ret
  .error:
  xor rax, rax
  ret

; rdi <- memory address to free (must be from malloc or NULL)
free:
  test rdi, rdi
  jz .end             ; free(NULL) is safe no-op, we will just return back

  mov rax, [rdi - 16] ; check for the signature added during malloc
  mov rsi, SIGNATURE
  cmp rax, rsi
  jne .err            ; signature mismatch: not our block from malloc

  mov rsi, [rdi - 8]  ; get our total size from the header from malloc
%ifdef TRACK_MEMORY
  inc QWORD[rel _mem_free_count]
  sub QWORD[rel _mem_active_bytes], rsi
%endif
  lea rdi, [rdi - 16] ; get base address
  EXEC_SYSCALL SYS_MUNMAP
  jmp .end
  .err:
  mov rdi, 2        ; arg for stderr
  mov rsi, free_err ; arg for src string
  call print_string
  .end:
  ret

; --- File I/O --- macro handles setting negative errno on failure

; rdi <- filename (null terminated)
; rsi <- flags (0 = O_RDONLY, O_WRONLY|O_CREAT|O_TRUNC: 1537 on macos, 577 on linux)
; rdx <- mode (permissions, 420 for 0644)
; rax -> file descriptor, or negative on error
sys_open:
  EXEC_SYSCALL SYS_OPEN
  ret

; rdi <- file descriptor
; rsi <- buffer address
; rdx <- number of bytes to read
; rax -> bytes read, or negative on error
sys_read:
  EXEC_SYSCALL SYS_READ
  ret

; rdi <- file descriptor
; rsi <- buffer address
; rdx <- number of bytes to write
; rax -> bytes written, or negative on error
sys_write:
  EXEC_SYSCALL SYS_WRITE
  ret

; rdi <- file descriptor
sys_close:
  EXEC_SYSCALL SYS_CLOSE
  ret

; rdi <- file descriptor
; rsi <- offset
; rdx <- whence (0 = SEEK_SET, 1 = SEEK_CUR, 2 = SEEK_END)
; rax -> new offset, or negative on error
sys_lseek:
  EXEC_SYSCALL SYS_LSEEK
  ret

; --- Memory wrappers for arena allocator ---
; * no error checking, no internal fragmentation from header overhead
; * that is found in the malloc procedure

; rdi <- size in bytes
; rax -> allocated memory address
sys_mmap:
  mov rsi, rdi       ; length = total_size
  xor rdi, rdi       ; addr = NULL (kernel)
  mov rdx, 3         ; prot = PROT_READ | PROT_WRITE
  mov r10, MAP_FLAGS ; flags = MAP_PRIVATE | MAP_ANON
  mov r8, -1         ; fd = -1
  xor r9, r9         ; offset = 0
  EXEC_SYSCALL SYS_MMAP
  ret

; rdi <- addr
; rsi <- len
sys_munmap:
  EXEC_SYSCALL SYS_MUNMAP
  ret

section .data
  clr_scr: db 0x1B, '[', '2', 'J', 0x1B, '[', 'H'  ; "\x1B[2J\x1B[H"
  free_err: db "[ASM Error] Invalid free of memory not allocated by 'malloc' / not 16-bit aligned", 10, 0
%ifdef TRACK_MEMORY
  leak_msg_start:  db 10, "=== HEAP SUMMARY:", 10, 0
  leak_msg_usage:  db "===   total heap usage: ", 0
  leak_msg_allocs: db " allocs, ", 0
  leak_msg_frees:  db " frees", 10, 0
  leak_msg_inuse:  db "===   in use at exit: ", 0
  leak_msg_bytes:  db " bytes", 10, 0
%endif

section .bss
%ifdef TRACK_MEMORY
  _mem_alloc_count  resq 1
  _mem_free_count   resq 1
  _mem_active_bytes resq 1
%endif
