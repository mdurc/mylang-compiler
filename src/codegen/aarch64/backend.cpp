#include "backend.h"

AArch64CodeGenerator::AArch64CodeGenerator(Logger* logger, TargetOS target)
    : m_logger(logger), m_target(target) {}

std::string AArch64CodeGenerator::generate(const std::vector<IRInstruction>& instructions,
                                           bool is_main_defined) {
  (void)m_logger;
  (void)m_target;
  (void)is_main_defined;
  (void)instructions;

  std::string asm_code;
  asm_code += ".text\n";
  asm_code += ".align 2\n";
  asm_code += ".globl _start\n";
  asm_code += "_start:\n";
  asm_code += "\tmov x0, #0\n";
  asm_code += "\tmov x16, #1\n";
  asm_code += "\tsvc #0x80\n";
  return asm_code;
}
