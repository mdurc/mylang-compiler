.text
.align 2

// --- test data ---
.data
msg_normal:     .asciz "hello, world\n"
msg_single:     .asciz "X\n"
msg_newline:    .asciz "\n"
msg_empty:      .asciz ""           // zero-length: only null terminator
msg_long:       .asciz "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@\n"
msg_special:    .asciz "tab:\there | newline below:\nnewline above\n"
msg_back2back_a:.asciz "back-to-back A\n"
msg_back2back_b:.asciz "back-to-back B\n"
msg_stderr:     .asciz "this goes to stderr\n"

hdr_normal:     .asciz "--- normal string ---\n"
hdr_single:     .asciz "--- single char ---\n"
hdr_newline:    .asciz "--- newline only ---\n"
hdr_empty:      .asciz "--- empty string (no output expected) ---\n"
hdr_long:       .asciz "--- long string ---\n"
hdr_special:    .asciz "--- special chars ---\n"
hdr_back2back:  .asciz "--- back-to-back calls ---\n"
hdr_stderr:     .asciz "--- stderr (fd=2) ---\n"
hdr_done:       .asciz "--- all tests complete ---\n"

.text
.align 2
.global _start
_start:
  // --- normal string ---
  mov x0, #1
  LOAD_ADDR x1, hdr_normal
  bl print_string

  mov x0, #1
  LOAD_ADDR x1, msg_normal
  bl print_string

  // --- single character ---
  mov x0, #1
  LOAD_ADDR x1, hdr_single
  bl print_string

  mov x0, #1
  LOAD_ADDR x1, msg_single
  bl print_string

  // --- newline only ---
  mov x0, #1
  LOAD_ADDR x1, hdr_newline
  bl print_string

  mov x0, #1
  LOAD_ADDR x1, msg_newline
  bl print_string

  // --- empty string ---
  // print_string should write 0 bytes; no output, no crash
  mov x0, #1
  LOAD_ADDR x1, hdr_empty
  bl print_string

  mov x0, #1
  LOAD_ADDR x1, msg_empty
  bl print_string

  // --- long string ---
  mov x0, #1
  LOAD_ADDR x1, hdr_long
  bl print_string

  mov x0, #1
  LOAD_ADDR x1, msg_long
  bl print_string

  // --- special characters ---
  mov x0, #1
  LOAD_ADDR x1, hdr_special
  bl print_string

  mov x0, #1
  LOAD_ADDR x1, msg_special
  bl print_string

  // --- back-to-back calls ---
  // verifies no register corruption across consecutive calls
  mov x0, #1
  LOAD_ADDR x1, hdr_back2back
  bl print_string

  mov x0, #1
  LOAD_ADDR x1, msg_back2back_a
  bl print_string

  mov x0, #1
  LOAD_ADDR x1, msg_back2back_b
  bl print_string

  // --- stderr ---
  // fd=2; output should appear on stderr, not mixed into stdout
  mov x0, #1
  LOAD_ADDR x1, hdr_stderr
  bl print_string

  mov x0, #2
  LOAD_ADDR x1, msg_stderr
  bl print_string

  // --- done ---
  mov x0, #1
  LOAD_ADDR x1, hdr_done
  bl print_string

  mov x0, #0
  bl exit
