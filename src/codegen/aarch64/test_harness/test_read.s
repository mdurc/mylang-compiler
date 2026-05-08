// -------------------------------------------------------
// Input piped: echo "   hello world\tfoo\tbar abc overflow_word" | ./a.out
//   "   hello world\tfoo\tbar abc overflow_word"
//
// Sequence:
//   1. single/leading-ws   -> "hello"
//   2. multiple #1         -> "world"
//   3. tab separated #1    -> "foo"
//   4. tab separated #2    -> "bar"
//   5. exact fit (buf=4)   -> "abc"
//   6. overflow (buf=4)    -> "overflow_word" -> expect null
// -------------------------------------------------------

.data

// ---- buffers ----
buf1:  .space 32
buf2:  .space 32
buf3:  .space 32
buf4:  .space 4      // small buffer for overflow test (holds 3 chars + null)

// ---- headers ----
hdr_single:    .asciz "--- single word ---\n"
hdr_multiple:  .asciz "--- multiple words ---\n"
hdr_leading:   .asciz "--- leading whitespace ---\n"
hdr_tabs:      .asciz "--- tab separated ---\n"
hdr_exact:     .asciz "--- exact buffer fit ---\n"
hdr_overflow:  .asciz "--- overflow (expect 0x0 / null) ---\n"
hdr_empty:     .asciz "--- empty input (expect 0x0 / null) ---\n"
hdr_done:      .asciz "--- all tests complete ---\n"

// ---- expected labels ----
expect_hello:     .asciz "expected: hello\n"
expect_world:     .asciz "expected: world\n"
expect_foo:       .asciz "expected: foo\n"
expect_bar:       .asciz "expected: bar\n"
expect_abc:       .asciz "expected: abc\n"
expect_null:      .asciz "expected: (null)\n"
lbl_got:          .asciz "got:      "
lbl_null:         .asciz "(null)\n"
lbl_newline:      .asciz "\n"

.text
.align 2

// print_result: prints "got: <word>\n" or "got: (null)\n"
// x0 <- pointer returned by read_word (0 = null)
print_result:
  stp x29, x30, [sp, #-32]!
  mov x29, sp
  str x19, [sp, #16]
  mov x19, x0
  mov x0, #1
  LOAD_ADDR x1, lbl_got
  bl print_string
  cmp x19, #0
  b.eq .Lpr_null
  mov x0, #1
  mov x1, x19
  bl print_string
  mov x0, #1
  bl print_newline
  b .Lpr_end
.Lpr_null:
  mov x0, #1
  LOAD_ADDR x1, lbl_null
  bl print_string
.Lpr_end:
  ldr x19, [sp, #16]
  ldp x29, x30, [sp], #32
  ret

.global _start
_start:
  // --- leading whitespace / single word ---
  mov x0, #1
  LOAD_ADDR x1, hdr_leading
  bl print_string
  mov x0, #1
  LOAD_ADDR x1, expect_hello
  bl print_string
  LOAD_ADDR x0, buf1
  mov x1, #32
  bl read_word
  bl print_result

  // --- multiple words: second word ---
  mov x0, #1
  LOAD_ADDR x1, hdr_multiple
  bl print_string
  mov x0, #1
  LOAD_ADDR x1, expect_world
  bl print_string
  LOAD_ADDR x0, buf2
  mov x1, #32
  bl read_word
  bl print_result

  // --- tab separated: foo ---
  mov x0, #1
  LOAD_ADDR x1, hdr_tabs
  bl print_string
  mov x0, #1
  LOAD_ADDR x1, expect_foo
  bl print_string
  LOAD_ADDR x0, buf3
  mov x1, #32
  bl read_word
  bl print_result

  // --- tab separated: bar ---
  mov x0, #1
  LOAD_ADDR x1, expect_bar
  bl print_string
  LOAD_ADDR x0, buf1
  mov x1, #32
  bl read_word
  bl print_result

  // --- exact buffer fit: "abc" into buf of size 4 (3 chars + null) ---
  mov x0, #1
  LOAD_ADDR x1, hdr_exact
  bl print_string
  mov x0, #1
  LOAD_ADDR x1, expect_abc
  bl print_string
  LOAD_ADDR x0, buf4
  mov x1, #4
  bl read_word
  bl print_result

  // --- overflow: "overflow_word" into buf of size 4 -> expect null ---
  mov x0, #1
  LOAD_ADDR x1, hdr_overflow
  bl print_string
  mov x0, #1
  LOAD_ADDR x1, expect_null
  bl print_string
  LOAD_ADDR x0, buf4
  mov x1, #4
  bl read_word
  bl print_result

  // --- done ---
  mov x0, #1
  LOAD_ADDR x1, hdr_done
  bl print_string

  mov x0, #0
  bl exit
