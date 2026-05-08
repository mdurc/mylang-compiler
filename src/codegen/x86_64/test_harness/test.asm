default rel

section .data
  ; ---- headers ----
  hdr_basic:      db "=== Basic I/O ===", 10, 0
  hdr_str:        db 10, "=== String Functions ===", 10, 0
  hdr_done:       db 10, "=== All tests complete ===", 10, 0

  ; ---- basic I/O strings ----
  msg_hello:      db "Hello, x86_64 world!", 10, 0
  msg_uint:       db "Testing print_uint (42): ", 0
  msg_int:        db "Testing print_int (-42): ", 0

  ; ---- string function tests ----
  str_hello:      db "hello", 0
  str_hello2:     db "hello", 0
  str_world:      db "world", 0

  msg_eq_1:       db "string_equals('hello', 'hello') expected: 1, got: ", 0
  msg_eq_0:       db "string_equals('hello', 'world') expected: 0, got: ", 0
  msg_concat:     db "string_concat('hello', 'world') expected: helloworld, got: ", 0
  msg_copy:       db "string_copy('world')          expected: world, got: ", 0

section .text
global _start

_start:
  mov rdi, 1
  lea rsi, [rel hdr_basic]
  call print_string

  mov rdi, 1
  lea rsi, [rel msg_hello]
  call print_string

  ; print positive unsigned int
  mov rdi, 1
  lea rsi, [rel msg_uint]
  call print_string
  mov rdi, 1
  mov rsi, 42
  call print_uint
  mov rdi, 1
  call print_newline

  ; print negative signed int
  mov rdi, 1
  lea rsi, [rel msg_int]
  call print_string
  mov rdi, 1
  mov rsi, -42
  call print_int
  mov rdi, 1
  call print_newline

  ; string manip
  mov rdi, 1
  lea rsi, [rel hdr_str]
  call print_string

  ; string_equals (expected 1)
  mov rdi, 1
  lea rsi, [rel msg_eq_1]
  call print_string
  lea rdi, [rel str_hello]
  lea rsi, [rel str_hello2]
  call string_equals
  mov rsi, rax
  mov rdi, 1
  call print_uint
  mov rdi, 1
  call print_newline

  ; string_equals (expected 0)
  mov rdi, 1
  lea rsi, [rel msg_eq_0]
  call print_string
  lea rdi, [rel str_hello]
  lea rsi, [rel str_world]
  call string_equals
  mov rsi, rax
  mov rdi, 1
  call print_uint
  mov rdi, 1
  call print_newline

  ; string_concat ("hello" + "world")
  mov rdi, 1
  lea rsi, [rel msg_concat]
  call print_string
  lea rdi, [rel str_hello]
  lea rsi, [rel str_world]
  call string_concat
  push rax
  mov rdi, 1
  mov rsi, rax
  call print_string
  mov rdi, 1
  call print_newline
  pop rdi
  call free

  ; string_copy ("world")
  mov rdi, 1
  lea rsi, [rel msg_copy]
  call print_string
  lea rdi, [rel str_world]
  call string_copy
  push rax
  mov rdi, 1
  mov rsi, rax
  call print_string
  mov rdi, 1
  call print_newline
  pop rdi
  call free

  mov rdi, 1
  lea rsi, [rel hdr_done]
  call print_string

  mov rdi, 0
  jmp exit
