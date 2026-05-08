// --- Embedded Runtime Library (AArch64) ---
.text
.align 2

.ifdef TARGET_MACOS
  .equ SYS_READ,   0x2000003
  .equ SYS_WRITE,  0x2000004
  .equ SYS_OPEN,   0x2000005
  .equ SYS_CLOSE,  0x2000006
  .equ SYS_EXIT,   0x2000001
  .equ SYS_MMAP,   0x20000C5
  .equ SYS_MUNMAP, 0x2000049
  .equ SYS_LSEEK,  0x20000C7
  .equ MAP_FLAGS,  0x1002   // MAP_PRIVATE | MAP_ANON
.else
  .equ SYS_READ,   63
  .equ SYS_WRITE,  64
  .equ SYS_OPEN,   56 // openat
  .equ SYS_CLOSE,  57
  .equ SYS_EXIT,   93
  .equ SYS_MMAP,   222
  .equ SYS_MUNMAP, 215
  .equ SYS_LSEEK,  62
  .equ MAP_FLAGS,  0x22   // MAP_PRIVATE | MAP_ANONYMOUS
.endif

// architecture-specific macro to check syscall success
// \sys_num: syscall number constant
.macro EXEC_SYSCALL sys_num
.ifdef TARGET_MACOS
  ldr x16, =\sys_num
  svc #0
  b.cc 1f    // branch if carry is clear (success)
  neg x0, x0 // macOS will set carry on error and returns positive errno -> negate to match linux
1:
.else
  mov x8, #\sys_num
  svc #0
.endif
.endm

// macro to load address of a data/bss label cross-platform
.macro LOAD_ADDR reg, label
.ifdef TARGET_MACOS
  adrp \reg, \label@PAGE
  add \reg, \reg, \label@PAGEOFF
.else
  adrp \reg, \label
  add \reg, \reg, :lo12:\label
.endif
.endm

// x0 <- exit code
// exits program with provided `x0` value
.global exit
exit:
.ifdef TRACK_MEMORY
  stp x29, x30, [sp, #-16]!
  mov x29, sp

  // save x0 exit code
  stp x0, x1, [sp, #-16]!

  // print exit code
  mov x0, #2
  LOAD_ADDR x1, leak_msg_exit
  bl print_string
  mov x0, #2
  ldr x1, [sp]
  bl print_uint

  bl _check_memory_leaks

  ldp x0, x1, [sp], #16
  ldp x29, x30, [sp], #16
.endif
  EXEC_SYSCALL SYS_EXIT

.ifdef TRACK_MEMORY
.global _check_memory_leaks
_check_memory_leaks:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  mov x0, #2
  LOAD_ADDR x1, leak_msg_start
  bl print_string
  mov x0, #2
  LOAD_ADDR x1, leak_msg_usage
  bl print_string
  mov x0, #2
  LOAD_ADDR x8, _mem_alloc_count
  ldr x1, [x8]
  bl print_uint
  mov x0, #2
  LOAD_ADDR x1, leak_msg_allocs
  bl print_string
  mov x0, #2
  LOAD_ADDR x8, _mem_free_count
  ldr x1, [x8]
  bl print_uint
  mov x0, #2
  LOAD_ADDR x1, leak_msg_frees
  bl print_string
  mov x0, #2
  LOAD_ADDR x1, leak_msg_inuse
  bl print_string
  mov x0, #2
  LOAD_ADDR x8, _mem_active_bytes
  ldr x1, [x8]
  bl print_uint
  mov x0, #2
  LOAD_ADDR x1, leak_msg_bytes
  bl print_string
  ldp x29, x30, [sp], #16
  ret
.endif

// x0 <- address of string
// x0 -> length of string
.global string_length
string_length:
  mov x1, #0              // initialize length to 0
1:
  ldrb w2, [x0, x1]       // load byte at x0 + x1
  cbz w2, 2f              // if byte is 0, jump forward to 2
  add x1, x1, #1
  b 1b
2:
  mov x0, x1              // return length in x0
  ret

// x0 <- file descriptor (1 stdout, 2 stderr)
// x1 <- address of null-terminated string
.global print_string
print_string:
  stp x29, x30, [sp, #-32]! // save fp/lr and make space for x19/x20
  mov x29, sp
  stp x19, x20, [sp, #16]

  mov x19, x0               // preserve fd
  mov x20, x1               // preserve str addr
  mov x0, x1                // prep arg for string_length (str addr)
  bl string_length
  mov x2, x0                // length is now size for SYS_WRITE
  mov x1, x20               // str addr
  mov x0, x19               // fd
  EXEC_SYSCALL SYS_WRITE

  // restore registers and stack
  ldp x19, x20, [sp, #16]
  ldp x29, x30, [sp], #32
  ret

// x0 <- file descriptor
// x1 <- char code
.global print_char
print_char:
  stp x29, x30, [sp, #-32]! // fp/lr and char-slot
  mov x29, sp
  str w1, [sp, #16]       // store char in slot
  add x1, sp, #16         // point x1 at it
  mov x2, #1              // length 1
  EXEC_SYSCALL SYS_WRITE
  ldp x29, x30, [sp], #32
  ret

// x0 <- file descriptor
// prints newline (0x0A) to fd
.global print_newline
print_newline:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  mov w1, #10             // 0x0A (newline)
  bl print_char
  ldp x29, x30, [sp], #16
  ret

.global clrscr
clrscr:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  mov x0, #1
  LOAD_ADDR x1, clr_scr_str
  mov x2, #7
  EXEC_SYSCALL SYS_WRITE
  ldp x29, x30, [sp], #16
  ret

// x0 <- file descriptor
// x1 <- unsigned 64-bit integer
.global print_uint
print_uint:
  // allocate 64 bytes total:
  // [sp, #0]  = x29, x30 (fp/link)
  // [sp, #16] = x19, x20 (callee-saved)
  // [sp, #32] to #63 = 32-byte string buffer
  stp x29, x30, [sp, #-64]!
  mov x29, sp
  stp x19, x20, [sp, #16]

  mov x19, x0          // preserve fd
  mov x20, x1          // preserve integer value

  add x4, sp, #63      // point to the end of the buffer
  strb wzr, [x4]       // plant null terminator (wzr is the zero register)

  mov x5, #10          // divisor
1:
  udiv x2, x20, x5     // x2 = quotient (value / 10)
  msub x3, x2, x5, x20 // x3 = remainder (value - (quotient * 10))
  add w3, w3, #'0'     // convert remainder to ASCII

  sub x4, x4, #1       // move buffer pointer left
  strb w3, [x4]        // store ASCII character

  mov x20, x2          // value = quotient
  cmp x20, #0
  b.ne 1b              // if value != 0, loop

  mov x0, x19          // fd
  mov x1, x4           // address of the string we just built
  bl print_string

  ldp x19, x20, [sp, #16]
  ldp x29, x30, [sp], #64
  ret

// x0 <- file descriptor
// x1 <- signed 64-bit integer
.global print_int
print_int:
  stp x29, x30, [sp, #-16]!
  mov x29, sp

  cmp x1, #0
  b.ge 1f              // if >= 0, just print as uint

  // it's negative. print '-' then print the negated number
  stp x0, x1, [sp, #-16]! // save args
  mov x1, #'-'
  bl print_char
  ldp x0, x1, [sp], #16   // restore args

  neg x1, x1           // make the integer positive
1:
  bl print_uint        // call print_uint with the positive value
  ldp x29, x30, [sp], #16
  ret

// returns character read from stdin in w0, else 0 on EOF
.global read_char
read_char:
  stp x29, x30, [sp, #-32]!
  mov x29, sp

  mov x0, #0           // fd 0 (stdin)
  add x1, sp, #16      // use safe stack space for the 1-byte buffer
  mov x2, #1           // read 1 byte
  EXEC_SYSCALL SYS_READ

  cmp x0, #0
  b.le 1f              // if bytes read <= 0, EOF or error
  ldrb w0, [sp, #16]   // load the character into w0
  b 2f
1:
  mov w0, #0           // return 0
2:
  ldp x29, x30, [sp], #32
  ret

// x0 <- buffer addr
// x1 <- buffer size
.global read_word
read_word:
  stp x29, x30, [sp, #-48]!
  mov x29, sp
  stp x19, x20, [sp, #16]
  str x21, [sp, #32]
  mov x19, x0         // buffer addr
  mov x20, x1         // buffer size
  mov x21, #0         // index
.Lskip_ws:
  bl read_char
  cmp w0, #0
  b.eq .Lrw_eof
  cmp w0, #0x20       // space
  b.eq .Lskip_ws
  cmp w0, #0x09       // tab
  b.eq .Lskip_ws
  cmp w0, #0x0A       // newline
  b.eq .Lskip_ws

.Lrw_loop:
  sub x4, x20, #1
  cmp x21, x4
  b.ge .Lrw_err       // overflow

  strb w0, [x19, x21]
  add x21, x21, #1

  bl read_char
  cmp w0, #0
  b.eq .Lrw_terminate
  cmp w0, #0x20
  b.eq .Lrw_terminate
  cmp w0, #0x09
  b.eq .Lrw_terminate
  cmp w0, #0x0A
  b.eq .Lrw_terminate
  b .Lrw_loop

.Lrw_eof:
  cmp x21, #0
  b.eq .Lrw_err
.Lrw_terminate:
  strb wzr, [x19, x21]
  mov x0, x19
  b .Lrw_end
.Lrw_err:
  mov x0, #0
.Lrw_end:
  ldr x21, [sp, #32]
  ldp x19, x20, [sp, #16]
  ldp x29, x30, [sp], #48
  ret

// x0 <- address of null-terminated string
// x0 -> parsed unsigned number
// x1 -> character count
.global parse_uint
parse_uint:
  mov x2, #0           // res (result)
  mov x1, #0           // character count
  mov x3, #10          // multiplier
1:
  ldrb w4, [x0, x1]    // load char
  cbz w4, 2f           // if null byte, done

  cmp w4, #'0'
  b.lt 2f              // if char < '0', done
  cmp w4, #'9'
  b.gt 2f              // if char > '9', done

  sub w4, w4, #'0'
  mul x2, x2, x3       // res *= 10
  add x2, x2, x4       // res += digit

  add x1, x1, #1
  b 1b
2:
  mov x0, x2
  ret

// x0 <- address of null-terminated string
// x0 -> parsed signed number
// x1 -> character count (including sign if any)
// no space between sign and digits are allowed.
.global parse_int
parse_int:
  stp x29, x30, [sp, #-32]!
  mov x29, sp

  stp x19, x20, [sp, #16]
  mov x19, x0          // save string pointer
  mov x20, #0          // sign flag (0 = no sign, 1 = negative, 2 = positive)

  ldrb w2, [x19]
  cmp w2, #'-'
  b.ne 1f
  mov x20, #1          // set negative flag
  add x19, x19, #1     // skip sign character
  b 2f
1:
  cmp w2, #'+'
  b.ne 2f
  mov x20, #2
  add x19, x19, #1     // skip sign character
2:
  mov x0, x19
  bl parse_uint        // call parse_uint on the rest of the string

  // x0 = number, x1 = length. we need to add the sign back to length if present.
  cmp x20, #0
  b.eq 3f
  add x1, x1, #1
  cmp x20, #1          // check if it was negative specifically
  b.ne 3f
  neg x0, x0           // negate number
3:
  ldp x19, x20, [sp, #16]
  ldp x29, x30, [sp], #32
  ret

// x0 <- address of string
// x1 <- address of string
// x0 -> 1 if they are equal, else 0
.global string_equals
string_equals:
  stp x29, x30, [sp, #-48]!
  mov x29, sp
  stp x19, x20, [sp, #16]
  stp x21, x22, [sp, #32]

  mov x19, x0
  mov x20, x1
  bl string_length
  mov x21, x0         // len1 saved in x21
  mov x0, x20
  bl string_length
  mov x22, x0         // len2 saved in x22

  cmp x21, x22
  b.ne .Lneq

  mov x4, #0          // index
1:
  ldrb w5, [x19, x4]
  ldrb w6, [x20, x4]
  cmp w5, w6
  b.ne .Lneq

  cbz w5, .Leq        // reached null byte cleanly
  add x4, x4, #1
  b 1b
.Leq:
  mov x0, #1
  b 2f
.Lneq:
  mov x0, #0
2:
  ldp x21, x22, [sp, #32]
  ldp x19, x20, [sp, #16]
  ldp x29, x30, [sp], #48
  ret

// x0 <- addr of null-terminated string
// x0 -> address of heap-allocated copy, or 0 on failure
// copies the string from x0 into a newly allocated buffer on the heap
.global string_copy
string_copy:
  stp x29, x30, [sp, #-32]!
  mov x29, sp
  stp x19, x20, [sp, #16]

  mov x19, x0 // src
  bl string_length
  add x20, x0, #1 // len + 1

  mov x0, x20
  bl malloc
  cmp x0, #0
  b.eq 1f

  mov x1, x19
  mov x2, x20
  bl memcpy
1:
  ldp x19, x20, [sp, #16]
  ldp x29, x30, [sp], #32
  ret

// x0 <- addr of first null-terminated str
// x1 <- addr of second null-terminated str
// x0 -> addr of new null-terminated str (heap-allocated; caller must free)
.global string_concat
string_concat:
  stp x29, x30, [sp, #-64]!
  mov x29, sp
  stp x19, x20, [sp, #16]
  stp x21, x22, [sp, #32]
  str x23, [sp, #48]

  mov x19, x0         // str1
  mov x20, x1         // str2
  bl string_length
  mov x21, x0         // len1

  mov x0, x20
  bl string_length
  mov x22, x0         // len2

  add x0, x21, x22
  add x0, x0, #1      // total size
  bl malloc
  cmp x0, #0
  b.eq 1f

  mov x23, x0
  mov x1, x19
  mov x2, x21
  bl memcpy

  add x0, x23, x21    // base + len1
  mov x1, x20
  mov x2, x22
  bl memcpy

  add x0, x23, x21
  add x0, x0, x22     // dst + len1 + len2
  strb wzr, [x0]      // null terminate
  mov x0, x23         // restore protected pointer to return
1:
  ldr x23, [sp, #48]
  ldp x21, x22, [sp, #32]
  ldp x19, x20, [sp, #16]
  ldp x29, x30, [sp], #64
  ret

// x0 <- dst
// x1 <- src
// x2 <- size
.global memcpy
memcpy:
  mov x3, #0 // index
1:
  cmp x3, x2
  b.ge 2f
  ldrb w4, [x1, x3]
  strb w4, [x0, x3]
  add x3, x3, #1
  b 1b
2:
  ret // x0 is still dst

// x0 <- dst
// x1 <- value
// x2 <- size
.global memset
memset:
  mov x3, #0
1:
  cmp x3, x2
  b.ge 2f
  strb w1, [x0, x3]
  add x3, x3, #1
  b 1b
2:
  ret

// x0 <- size in bytes
// returns allocated memory address (16-byte aligned), or 0 on error
.global malloc
malloc:
  stp x29, x30, [sp, #-32]!
  mov x29, sp
  stp x19, x20, [sp, #16]

  cmp x0, #0
  b.eq .Lmalloc_err

  add x19, x0, #16    // x19 = total_size = requested_size + 16

  mov x0, x19
  bl sys_mmap

  // check if mmap failed (returns -1)
  cmn x0, #1
  b.eq .Lmalloc_err

  // write signature 0xDEADC0DEDEADC0DE (done in 16-bit chunks)
  movz x8, #0xC0DE
  movk x8, #0xDEAD, lsl #16
  movk x8, #0xC0DE, lsl #32
  movk x8, #0xDEAD, lsl #48

  str x8, [x0]        // signature at offset 0
  str x19, [x0, #8]   // total size at offset 8

.ifdef TRACK_MEMORY
  LOAD_ADDR x8, _mem_alloc_count
  ldr x9, [x8]
  add x9, x9, #1
  str x9, [x8]

  LOAD_ADDR x8, _mem_active_bytes
  ldr x9, [x8]
  add x9, x9, x19
  str x9, [x8]
.endif

  add x0, x0, #16     // return user pointer (bypassing 16-byte header)
  b .Lmalloc_end

.Lmalloc_err:
  mov x0, #0
.Lmalloc_end:
  ldp x19, x20, [sp, #16]
  ldp x29, x30, [sp], #32
  ret

// x0 <- memory address to free
.global free
free:
  cmp x0, #0
  b.eq 2f             // free(NULL) is a safe no-op

  // check signature
  sub x1, x0, #16
  ldr x8, [x1]        // load signature
  movz x9, #0xC0DE
  movk x9, #0xDEAD, lsl #16
  movk x9, #0xC0DE, lsl #32
  movk x9, #0xDEAD, lsl #48

  cmp x8, x9
  b.ne .Lfree_err

  ldr x2, [x1, #8]    // use x2 (scratch) to load total size

.ifdef TRACK_MEMORY
  LOAD_ADDR x8, _mem_free_count
  ldr x9, [x8]
  add x9, x9, #1
  str x9, [x8]

  LOAD_ADDR x8, _mem_active_bytes
  ldr x9, [x8]
  sub x9, x9, x2      // subtract size using x2
  str x9, [x8]
.endif

  mov x0, x1          // original mmap addr
  mov x1, x2          // original length

  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl sys_munmap
  ldp x29, x30, [sp], #16
  b 2f

.Lfree_err:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  mov x0, #2 // stderr
  LOAD_ADDR x1, free_err_msg
  bl print_string
  ldp x29, x30, [sp], #16
2:
  ret

// --- File I/O ---

// x0 <- filename (null terminated)
// x1 <- flags
// x2 <- mode (permissions)
.global sys_open
sys_open:
.ifdef TARGET_MACOS
  EXEC_SYSCALL SYS_OPEN
  ret
.else
  // linux AArch64 natively uses openat(AT_FDCWD, filename, flags, mode)
  mov x3, x2              // mode
  mov x2, x1              // flags
  mov x1, x0              // filename
  mov x0, #-100           // AT_FDCWD
  EXEC_SYSCALL SYS_OPEN
  ret
.endif

// x0 <- file descriptor
// x1 <- buffer address
// x2 <- number of bytes to read
.global sys_read
sys_read:
  EXEC_SYSCALL SYS_READ
  ret

// x0 <- file descriptor
// x1 <- buffer address
// x2 <- number of bytes to write
.global sys_write
sys_write:
  EXEC_SYSCALL SYS_WRITE
  ret

// x0 <- file descriptor
.global sys_close
sys_close:
  EXEC_SYSCALL SYS_CLOSE
  ret

// x0 <- file descriptor
// x1 <- offset
// x2 <- whence (0 = SEEK_SET, 1 = SEEK_CUR, 2 = SEEK_END)
.global sys_lseek
sys_lseek:
  EXEC_SYSCALL SYS_LSEEK
  ret

// x0 <- size in bytes
// returns allocated memory address in x0
.global sys_mmap
sys_mmap:
  mov x1, x0          // length
  mov x0, #0          // addr = NULL
  mov x2, #3          // prot = PROT_READ | PROT_WRITE
  mov x3, MAP_FLAGS   // flags
  mov x4, #-1         // fd = -1
  mov x5, #0          // offset = 0
  EXEC_SYSCALL SYS_MMAP
  ret

// x0 <- addr
// x1 <- len
.global sys_munmap
sys_munmap:
  EXEC_SYSCALL SYS_MUNMAP
  ret

// data section
.data
.align 3
clr_scr_str: .ascii "\033[2J\033[H"
free_err_msg: .asciz "[ASM Error] Invalid free of memory not allocated by 'malloc' / not 16-byte aligned\n"

.ifdef TRACK_MEMORY
leak_msg_start:  .asciz "\n=== HEAP SUMMARY:\n"
leak_msg_usage:  .asciz "===   total heap usage: "
leak_msg_allocs: .asciz " allocs, "
leak_msg_frees:  .asciz " frees\n"
leak_msg_inuse:  .asciz "===   in use at exit: "
leak_msg_bytes:  .asciz " bytes\n"
leak_msg_exit:   .asciz "=== exit status: "
.endif

// globals section
.bss
.align 3
.ifdef TRACK_MEMORY
.global _mem_alloc_count
_mem_alloc_count:  .space 8
.global _mem_free_count
_mem_free_count:   .space 8
.global _mem_active_bytes
_mem_active_bytes: .space 8
.endif
