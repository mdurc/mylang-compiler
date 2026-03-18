#include <gtest/gtest.h>

#include "vendor/ApprovalTests.hpp"
#include "util.h"

TEST(ParserTests, ParserVarDecl) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--ast", "./samples/var_decl.sn"));
}

TEST(ParserTests, ParserControlFlow) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--ast", "./samples/control_flow.sn"));
}

TEST(ParserTests, ParserFunctions) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--ast", "./samples/functions.sn"));
}

TEST(ParserTests, ParserStructs) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--ast", "./samples/structs.sn"));
}

TEST(ParserTests, ParserPointers) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--ast", "./samples/pointers.sn"));
}

TEST(ParserTests, ParserAsmAndErrors) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--ast", "./samples/asm_and_errors.sn"));
}

TEST(ParserTests, ParserStdin) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--ast", "./samples/stdin.sn"));
}

TEST(ParserTests, ParserEmpty) {
  ApprovalTests::Approvals::verify(
      generate_compiler_output("--ast", "./samples/empty.sn"));
}
