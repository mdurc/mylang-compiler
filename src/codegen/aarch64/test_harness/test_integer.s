.data
hdr_uint_basic:  .asciz "--- print_uint: basic values ---\n"
hdr_uint_zero:   .asciz "--- print_uint: zero ---\n"
hdr_uint_max:    .asciz "--- print_uint: UINT64_MAX ---\n"
hdr_uint_pow10:  .asciz "--- print_uint: powers of 10 ---\n"
hdr_int_pos:     .asciz "--- print_int: positive ---\n"
hdr_int_neg:     .asciz "--- print_int: negative ---\n"
hdr_int_zero:    .asciz "--- print_int: zero ---\n"
hdr_int_min:     .asciz "--- print_int: INT64_MIN ---\n"
hdr_int_max:     .asciz "--- print_int: INT64_MAX ---\n"
hdr_int_neg1:    .asciz "--- print_int: -1 ---\n"
hdr_done:        .asciz "--- all tests complete ---\n"

expect_0:        .asciz "expected: 0\n"
expect_1:        .asciz "expected: 1\n"
expect_42:       .asciz "expected: 42\n"
expect_255:      .asciz "expected: 255\n"
expect_uint_max: .asciz "expected: 18446744073709551615\n"
expect_int_max:  .asciz "expected: 9223372036854775807\n"
expect_int_min:  .asciz "expected: -9223372036854775808\n"
expect_neg1:     .asciz "expected: -1\n"
expect_neg42:    .asciz "expected: -42\n"

.text
.align 2
.global _start
_start:

  // =========================================================
  // print_uint
  // =========================================================

  mov x0, #1
  LOAD_ADDR x1, hdr_uint_basic
  bl print_string

  // 1
  mov x0, #1
  LOAD_ADDR x1, expect_1
  bl print_string
  mov x0, #1
  mov x1, #1
  bl print_uint
  mov x0, #1
  bl print_newline

  // 42
  mov x0, #1
  LOAD_ADDR x1, expect_42
  bl print_string
  mov x0, #1
  mov x1, #42
  bl print_uint
  mov x0, #1
  bl print_newline

  // 255
  mov x0, #1
  LOAD_ADDR x1, expect_255
  bl print_string
  mov x0, #1
  mov x1, #255
  bl print_uint
  mov x0, #1
  bl print_newline

  // zero
  mov x0, #1
  LOAD_ADDR x1, hdr_uint_zero
  bl print_string
  mov x0, #1
  LOAD_ADDR x1, expect_0
  bl print_string
  mov x0, #1
  mov x1, #0
  bl print_uint
  mov x0, #1
  bl print_newline

  // UINT64_MAX
  mov x0, #1
  LOAD_ADDR x1, hdr_uint_max
  bl print_string
  mov x0, #1
  LOAD_ADDR x1, expect_uint_max
  bl print_string
  mov x0, #1
  mov x1, #0
  mvn x1, x1
  bl print_uint
  mov x0, #1
  bl print_newline

  // powers of 10
  mov x0, #1
  LOAD_ADDR x1, hdr_uint_pow10
  bl print_string

  mov x0, #1
  mov x1, #1
  bl print_uint
  mov x0, #1
  bl print_newline

  mov x0, #1
  mov x1, #10
  bl print_uint
  mov x0, #1
  bl print_newline

  mov x0, #1
  mov x1, #100
  bl print_uint
  mov x0, #1
  bl print_newline

  mov x0, #1
  mov x1, #1000
  bl print_uint
  mov x0, #1
  bl print_newline

  mov x0, #1
  ldr x1, =1000000000
  bl print_uint
  mov x0, #1
  bl print_newline

  mov x0, #1
  ldr x1, =1000000000000
  bl print_uint
  mov x0, #1
  bl print_newline

  // =========================================================
  // print_int
  // =========================================================

  // positive
  mov x0, #1
  LOAD_ADDR x1, hdr_int_pos
  bl print_string
  mov x0, #1
  LOAD_ADDR x1, expect_1
  bl print_string
  mov x0, #1
  mov x1, #1
  bl print_int
  mov x0, #1
  bl print_newline

  // zero
  mov x0, #1
  LOAD_ADDR x1, hdr_int_zero
  bl print_string
  mov x0, #1
  LOAD_ADDR x1, expect_0
  bl print_string
  mov x0, #1
  mov x1, #0
  bl print_int
  mov x0, #1
  bl print_newline

  // -1
  mov x0, #1
  LOAD_ADDR x1, hdr_int_neg1
  bl print_string
  mov x0, #1
  LOAD_ADDR x1, expect_neg1
  bl print_string
  mov x0, #1
  mov x1, #-1
  bl print_int
  mov x0, #1
  bl print_newline

  // -42
  mov x0, #1
  LOAD_ADDR x1, hdr_int_neg
  bl print_string
  mov x0, #1
  LOAD_ADDR x1, expect_neg42
  bl print_string
  mov x0, #1
  mov x1, #-42
  bl print_int
  mov x0, #1
  bl print_newline

  // INT64_MAX
  mov x0, #1
  LOAD_ADDR x1, hdr_int_max
  bl print_string
  mov x0, #1
  LOAD_ADDR x1, expect_int_max
  bl print_string
  mov x0, #1
  mov x1, #0x7FFFFFFFFFFFFFFF
  bl print_int
  mov x0, #1
  bl print_newline

  // INT64_MIN
  mov x0, #1
  LOAD_ADDR x1, hdr_int_min
  bl print_string
  mov x0, #1
  LOAD_ADDR x1, expect_int_min
  bl print_string
  mov x0, #1
  mov x1, #0x8000000000000000
  bl print_int
  mov x0, #1
  bl print_newline

  // done
  mov x0, #1
  LOAD_ADDR x1, hdr_done
  bl print_string

  mov x0, #0
  bl exit
