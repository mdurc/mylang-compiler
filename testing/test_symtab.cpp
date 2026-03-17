#include <gtest/gtest.h>

#include "vendor/ApprovalTests.hpp"
#include "util.h"

TEST(SymTabTests, SymTabVarDecl) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--symtab", "./samples/var_decl.sn"));
}

TEST(SymTabTests, SymTabControlFlow) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--symtab", "./samples/control_flow.sn"));
}

TEST(SymTabTests, SymTabFunctions) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--symtab", "./samples/functions.sn"));
}

TEST(SymTabTests, SymTabStructs) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--symtab", "./samples/structs.sn"));
}

TEST(SymTabTests, SymTabPointers) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--symtab", "./samples/pointers.sn"));
}

TEST(SymTabTests, SymTabAsmAndErrors) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--symtab", "./samples/asm_and_errors.sn"));
}

TEST(SymTabTests, SymTabStdin) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--symtab", "./samples/stdin.sn"));
}
