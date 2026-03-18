#include <gtest/gtest.h>

#include "vendor/ApprovalTests.hpp"
#include "util.h"

TEST(LexerTests, LexerVarDecl) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--tokens", "./samples/var_decl.sn"));
}

TEST(LexerTests, LexerControlFlow) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--tokens", "./samples/control_flow.sn"));
}

TEST(LexerTests, LexerFunctions) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--tokens", "./samples/functions.sn"));
}

TEST(LexerTests, LexerStructs) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--tokens", "./samples/structs.sn"));
}
TEST(LexerTests, LexerPointers) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--tokens", "./samples/pointers.sn"));
}

TEST(LexerTests, LexerAsmAndErrors) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--tokens", "./samples/asm_and_errors.sn"));
}

TEST(LexerTests, LexerStdin) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--tokens", "./samples/stdin.sn"));
}

TEST(LexerTests, LexerEmpty) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--tokens", "./samples/empty.sn"));
}
