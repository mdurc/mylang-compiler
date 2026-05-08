.data

// ---- buffers ----
str_zero:        .asciz "0"
str_one:         .asciz "1"
str_pos:         .asciz "42"
str_neg:         .asciz "-42"
str_plus:        .asciz "+42"
str_uint_max:    .asciz "18446744073709551615"
str_int_max:     .asciz "9223372036854775807"
str_int_min:     .asciz "-9223372036854775808"
str_leading:     .asciz "007"
str_no_digits:   .asciz "abc"
str_sign_only:   .asciz "-"
str_empty:       .asciz ""
str_mixed:       .asciz "123abc"

// ---- headers ----
hdr_uint:        .asciz "--- parse_uint ---\n"
hdr_int:         .asciz "--- parse_int ---\n"
hdr_done:        .asciz "--- all tests complete ---\n"

// ---- expected ----
expect_0_c0:     .asciz "expected: val=0 count=0\n"
expect_0_c1:     .asciz "expected: val=0 count=1\n"
expect_1_c1:     .asciz "expected: val=1 count=1\n"
expect_42_c2:    .asciz "expected: val=42 count=2\n"
expect_n42_c3:   .asciz "expected: val=-42 count=3\n"
expect_p42_c3:   .asciz "expected: val=42 count=3\n"
expect_umax_c20: .asciz "expected: val=18446744073709551615 count=20\n"
expect_imax_c19: .asciz "expected: val=9223372036854775807 count=19\n"
expect_imin_c20: .asciz "expected: val=-9223372036854775808 count=20\n"
expect_7_c3:     .asciz "expected: val=7 count=3\n"
expect_123_c3:   .asciz "expected: val=123 count=3\n"

lbl_val:         .asciz "got:      val="
lbl_count:       .asciz " count="
lbl_newline:     .asciz "\n"

.text
.align 2

// print_result: prints "got: val=<x19> count=<x20>\n"
// x19 <- value (signed for int, unsigned for uint)
// x20 <- count
// x21 <- 0 for unsigned, 1 for signed
print_result:
  stp x29, x30, [sp, #-48]!
  mov x29, sp
  stp x19, x20, [sp, #16]
  str x21, [sp, #32]
  mov x19, x0
  mov x20, x1
  mov x21, x2
  mov x0, #1
  LOAD_ADDR x1, lbl_val
  bl print_string
  cmp x21, #1
  b.eq .Lpr_signed
  mov x0, #1
  mov x1, x19
  bl print_uint
  b .Lpr_count
.Lpr_signed:
  mov x0, #1
  mov x1, x19
  bl print_int
.Lpr_count:
  mov x0, #1
  LOAD_ADDR x1, lbl_count
  bl print_string
  mov x0, #1
  mov x1, x20
  bl print_uint
  mov x0, #1
  bl print_newline
  ldr x21, [sp, #32]
  ldp x19, x20, [sp, #16]
  ldp x29, x30, [sp], #48
  ret

.global _start
_start:

  // =========================================================
  // parse_uint
  // =========================================================

  mov x0, #1
  LOAD_ADDR x1, hdr_uint
  bl print_string

  // zero
  mov x0, #1
  LOAD_ADDR x1, expect_0_c1
  bl print_string
  LOAD_ADDR x0, str_zero
  bl parse_uint
  mov x2, #0
  bl print_result

  // one
  mov x0, #1
  LOAD_ADDR x1, expect_1_c1
  bl print_string
  LOAD_ADDR x0, str_one
  bl parse_uint
  mov x2, #0
  bl print_result

  // 42
  mov x0, #1
  LOAD_ADDR x1, expect_42_c2
  bl print_string
  LOAD_ADDR x0, str_pos
  bl parse_uint
  mov x2, #0
  bl print_result

  // UINT64_MAX
  mov x0, #1
  LOAD_ADDR x1, expect_umax_c20
  bl print_string
  LOAD_ADDR x0, str_uint_max
  bl parse_uint
  mov x2, #0
  bl print_result

  // leading zeros: "007" -> val=7 count=3
  mov x0, #1
  LOAD_ADDR x1, expect_7_c3
  bl print_string
  LOAD_ADDR x0, str_leading
  bl parse_uint
  mov x2, #0
  bl print_result

  // no digits: "abc" -> val=0 count=0
  mov x0, #1
  LOAD_ADDR x1, expect_0_c0
  bl print_string
  LOAD_ADDR x0, str_no_digits
  bl parse_uint
  mov x2, #0
  bl print_result

  // empty string -> val=0 count=0
  mov x0, #1
  LOAD_ADDR x1, expect_0_c0
  bl print_string
  LOAD_ADDR x0, str_empty
  bl parse_uint
  mov x2, #0
  bl print_result

  // mixed "123abc" -> val=123 count=3 (stops at non-digit)
  mov x0, #1
  LOAD_ADDR x1, expect_123_c3
  bl print_string
  LOAD_ADDR x0, str_mixed
  bl parse_uint
  mov x2, #0
  bl print_result

  // =========================================================
  // parse_int
  // =========================================================

  mov x0, #1
  LOAD_ADDR x1, hdr_int
  bl print_string

  // "42" -> val=42 count=2
  mov x0, #1
  LOAD_ADDR x1, expect_42_c2
  bl print_string
  LOAD_ADDR x0, str_pos
  bl parse_int
  mov x2, #1
  bl print_result

  // "-42" -> val=-42 count=3
  mov x0, #1
  LOAD_ADDR x1, expect_n42_c3
  bl print_string
  LOAD_ADDR x0, str_neg
  bl parse_int
  mov x2, #1
  bl print_result

  // "+42" -> val=42 count=3
  mov x0, #1
  LOAD_ADDR x1, expect_p42_c3
  bl print_string
  LOAD_ADDR x0, str_plus
  bl parse_int
  mov x2, #1
  bl print_result

  // "0" -> val=0 count=1
  mov x0, #1
  LOAD_ADDR x1, expect_0_c1
  bl print_string
  LOAD_ADDR x0, str_zero
  bl parse_int
  mov x2, #1
  bl print_result

  // INT64_MAX
  mov x0, #1
  LOAD_ADDR x1, expect_imax_c19
  bl print_string
  LOAD_ADDR x0, str_int_max
  bl parse_int
  mov x2, #1
  bl print_result

  // INT64_MIN
  mov x0, #1
  LOAD_ADDR x1, expect_imin_c20
  bl print_string
  LOAD_ADDR x0, str_int_min
  bl parse_int
  mov x2, #1
  bl print_result

  // "-" alone -> val=0 count=1 (sign consumed, no digits)
  mov x0, #1
  LOAD_ADDR x1, expect_0_c1
  bl print_string
  LOAD_ADDR x0, str_sign_only
  bl parse_int
  mov x2, #1
  bl print_result

  // "abc" -> val=0 count=0
  mov x0, #1
  LOAD_ADDR x1, expect_0_c0
  bl print_string
  LOAD_ADDR x0, str_no_digits
  bl parse_int
  mov x2, #1
  bl print_result

  // done
  mov x0, #1
  LOAD_ADDR x1, hdr_done
  bl print_string

  mov x0, #0
  bl exit
