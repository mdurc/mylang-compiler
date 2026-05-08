.data

// ---- test strings ----
str_hello:      .asciz "hello"
str_hello2:     .asciz "hello"
str_world:      .asciz "world"
str_empty:      .asciz ""
str_empty2:     .asciz ""
str_a:          .asciz "a"
str_a2:         .asciz "a"
str_b:          .asciz "b"
str_foo:        .asciz "foo"
str_foobar:     .asciz "foobar"
str_space:      .asciz " "

// ---- headers ----
hdr_equals:     .asciz "--- string_equals ---\n"
hdr_copy:       .asciz "--- string_copy ---\n"
hdr_concat:     .asciz "--- string_concat ---\n"
hdr_done:       .asciz "--- all tests complete ---\n"

// ---- expected ----
expect_1:       .asciz "expected: 1\n"
expect_0:       .asciz "expected: 0\n"
lbl_got:        .asciz "got:      "
lbl_newline:    .asciz "\n"

// ---- concat expected outputs ----
expect_helloworld:   .asciz "expected: helloworld\n"
expect_hello:        .asciz "expected: hello\n"
expect_world:        .asciz "expected: world\n"
expect_empty:        .asciz "expected: (empty)\n"
expect_foobar:       .asciz "expected: foobar\n"
expect_copy_hello:   .asciz "expected: hello (copy)\n"
expect_copy_empty:   .asciz "expected: (empty copy)\n"
expect_null:         .asciz "expected: (null - malloc fail not testable here)\n"
lbl_empty_str:       .asciz "(empty)\n"
lbl_null_str:        .asciz "(null)\n"

str_helloworld_ref: .asciz "helloworld"
str_bar:            .asciz "bar"

.text
.align 2

// print_equals_result: prints "got: 1" or "got: 0"
// x0 <- result from string_equals
print_equals_result:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  mov x1, x0
  mov x0, #1
  LOAD_ADDR x1, lbl_got
  bl print_string
  // re-load result — it was clobbered by print_string call
  // caller must pass via stack; simpler to just re-examine via print_uint
  ldp x29, x30, [sp], #16
  ret

// print_str_result: prints "got: <str>\n" or "got: (null)\n" or "got: (empty)\n"
// x0 <- string pointer (0 = null)
print_str_result:
  stp x29, x30, [sp, #-32]!
  mov x29, sp
  str x19, [sp, #16]
  mov x19, x0
  mov x0, #1
  LOAD_ADDR x1, lbl_got
  bl print_string
  cmp x19, #0
  b.eq .Lpsr_null
  // check if empty
  ldrb w2, [x19]
  cbz w2, .Lpsr_empty
  mov x0, #1
  mov x1, x19
  bl print_string
  mov x0, #1
  bl print_newline
  b .Lpsr_end
.Lpsr_null:
  mov x0, #1
  LOAD_ADDR x1, lbl_null_str
  bl print_string
  b .Lpsr_end
.Lpsr_empty:
  mov x0, #1
  LOAD_ADDR x1, lbl_empty_str
  bl print_string
.Lpsr_end:
  ldr x19, [sp, #16]
  ldp x29, x30, [sp], #32
  ret

.global _start
_start:

  // =========================================================
  // string_equals
  // =========================================================

  mov x0, #1
  LOAD_ADDR x1, hdr_equals
  bl print_string

  // equal strings
  mov x0, #1
  LOAD_ADDR x1, expect_1
  bl print_string
  LOAD_ADDR x0, str_hello
  LOAD_ADDR x1, str_hello2
  bl string_equals
  mov x1, x0
  mov x0, #1
  bl print_uint
  mov x0, #1
  bl print_newline

  // same pointer both args
  mov x0, #1
  LOAD_ADDR x1, expect_1
  bl print_string
  LOAD_ADDR x0, str_hello
  LOAD_ADDR x1, str_hello
  bl string_equals
  mov x1, x0
  mov x0, #1
  bl print_uint
  mov x0, #1
  bl print_newline

  // different strings
  mov x0, #1
  LOAD_ADDR x1, expect_0
  bl print_string
  LOAD_ADDR x0, str_hello
  LOAD_ADDR x1, str_world
  bl string_equals
  mov x1, x0
  mov x0, #1
  bl print_uint
  mov x0, #1
  bl print_newline

  // prefix vs full: "foo" vs "foobar"
  mov x0, #1
  LOAD_ADDR x1, expect_0
  bl print_string
  LOAD_ADDR x0, str_foo
  LOAD_ADDR x1, str_foobar
  bl string_equals
  mov x1, x0
  mov x0, #1
  bl print_uint
  mov x0, #1
  bl print_newline

  // both empty
  mov x0, #1
  LOAD_ADDR x1, expect_1
  bl print_string
  LOAD_ADDR x0, str_empty
  LOAD_ADDR x1, str_empty2
  bl string_equals
  mov x1, x0
  mov x0, #1
  bl print_uint
  mov x0, #1
  bl print_newline

  // empty vs non-empty
  mov x0, #1
  LOAD_ADDR x1, expect_0
  bl print_string
  LOAD_ADDR x0, str_empty
  LOAD_ADDR x1, str_hello
  bl string_equals
  mov x1, x0
  mov x0, #1
  bl print_uint
  mov x0, #1
  bl print_newline

  // single char equal
  mov x0, #1
  LOAD_ADDR x1, expect_1
  bl print_string
  LOAD_ADDR x0, str_a
  LOAD_ADDR x1, str_a2
  bl string_equals
  mov x1, x0
  mov x0, #1
  bl print_uint
  mov x0, #1
  bl print_newline

  // single char not equal
  mov x0, #1
  LOAD_ADDR x1, expect_0
  bl print_string
  LOAD_ADDR x0, str_a
  LOAD_ADDR x1, str_b
  bl string_equals
  mov x1, x0
  mov x0, #1
  bl print_uint
  mov x0, #1
  bl print_newline

  // =========================================================
  // string_copy
  // =========================================================

  mov x0, #1
  LOAD_ADDR x1, hdr_copy
  bl print_string

  // copy "hello"
  mov x0, #1
  LOAD_ADDR x1, expect_copy_hello
  bl print_string
  LOAD_ADDR x0, str_hello
  bl string_copy
  mov x19, x0
  bl print_str_result
  mov x0, x19
  bl free

  // copy empty string
  mov x0, #1
  LOAD_ADDR x1, expect_copy_empty
  bl print_string
  LOAD_ADDR x0, str_empty
  bl string_copy
  mov x19, x0
  bl print_str_result
  mov x0, x19
  bl free

  // copy then verify equality with original
  mov x0, #1
  LOAD_ADDR x1, expect_1
  bl print_string
  LOAD_ADDR x0, str_world
  bl string_copy
  mov x19, x0
  // x0 = copy ptr; compare with original
  LOAD_ADDR x1, str_world
  mov x0, x19
  bl string_equals
  mov x20, x0
  mov x0, x19
  bl free
  mov x1, x20
  mov x0, #1
  bl print_uint
  mov x0, #1
  bl print_newline

  // =========================================================
  // string_concat
  // =========================================================

  mov x0, #1
  LOAD_ADDR x1, hdr_concat
  bl print_string

  // "hello" + "world" = "helloworld"
  mov x0, #1
  LOAD_ADDR x1, expect_helloworld
  bl print_string
  LOAD_ADDR x0, str_hello
  LOAD_ADDR x1, str_world
  bl string_concat
  mov x19, x0
  bl print_str_result
  mov x0, x19
  bl free

  // "hello" + "" = "hello"
  mov x0, #1
  LOAD_ADDR x1, expect_hello
  bl print_string
  LOAD_ADDR x0, str_hello
  LOAD_ADDR x1, str_empty
  bl string_concat
  mov x19, x0
  bl print_str_result
  mov x0, x19
  bl free

  // "" + "world" = "world"
  mov x0, #1
  LOAD_ADDR x1, expect_world
  bl print_string
  LOAD_ADDR x0, str_empty
  LOAD_ADDR x1, str_world
  bl string_concat
  mov x19, x0
  bl print_str_result
  mov x0, x19
  bl free

  // "" + "" = ""
  mov x0, #1
  LOAD_ADDR x1, expect_empty
  bl print_string
  LOAD_ADDR x0, str_empty
  LOAD_ADDR x1, str_empty2
  bl string_concat
  mov x19, x0
  bl print_str_result
  mov x0, x19
  bl free

  // "foo" + "bar" = "foobar"
  mov x0, #1
  LOAD_ADDR x1, expect_foobar
  bl print_string
  LOAD_ADDR x0, str_foo
  LOAD_ADDR x1, str_bar
  bl string_concat
  mov x19, x0
  bl print_str_result
  mov x0, x19
  bl free

  // concat then verify with string_equals
  mov x0, #1
  LOAD_ADDR x1, expect_1
  bl print_string
  LOAD_ADDR x0, str_hello
  LOAD_ADDR x1, str_world
  bl string_concat
  mov x19, x0
  // x0 = "helloworld" heap copy
  LOAD_ADDR x1, str_helloworld_ref
  mov x0, x19
  bl string_equals
  mov x20, x0
  mov x0, x19
  bl free
  mov x1, x20
  mov x0, #1
  bl print_uint
  mov x0, #1
  bl print_newline

  // done
  mov x0, #1
  LOAD_ADDR x1, hdr_done
  bl print_string

  mov x0, #0
  bl exit
