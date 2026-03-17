#include <gtest/gtest.h>

#include "vendor/ApprovalTests.hpp"
#include "util.h"

TEST(IrTests, IrVarDecl) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--ir", "./samples/var_decl.sn"));
}

TEST(IrTests, IrControlFlow) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--ir", "./samples/control_flow.sn"));
}

TEST(IrTests, IrFunctions) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--ir", "./samples/functions.sn"));
}

TEST(IrTests, IrStructs) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--ir", "./samples/structs.sn"));
}

TEST(IrTests, IrPointers) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--ir", "./samples/pointers.sn"));
}

TEST(IrTests, IrAsmAndErrors) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--ir", "./samples/asm_and_errors.sn"));
}

TEST(IrTests, IrStdin) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--ir", "./samples/stdin.sn"));
}
