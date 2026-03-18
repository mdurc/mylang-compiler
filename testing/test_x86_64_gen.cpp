#include <gtest/gtest.h>

#include "vendor/ApprovalTests.hpp"
#include "util.h"

TEST(x86GenTests, x86GenVarDecl) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--asm", "./samples/var_decl.sn"));
}

TEST(x86GenTests, x86GenControlFlow) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--asm", "./samples/control_flow.sn"));
}

TEST(x86GenTests, x86GenFunctions) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--asm", "./samples/functions.sn"));
}

TEST(x86GenTests, x86GenStructs) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--asm", "./samples/structs.sn"));
}

TEST(x86GenTests, x86GenPointers) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--asm", "./samples/pointers.sn"));
}

TEST(x86GenTests, x86GenAsmAndErrors) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--asm", "./samples/asm_and_errors.sn"));
}

TEST(x86GenTests, x86GenStdin) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--asm", "./samples/stdin.sn"));
}

TEST(x86GenTests, x86GenEmpty) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--asm", "./samples/empty.sn"));
}
