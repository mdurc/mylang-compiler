#ifndef CODEGEN_AARCH64_BACKEND_H
#define CODEGEN_AARCH64_BACKEND_H

#include <string>
#include <vector>

#include "../../logging/logger.h"
#include "../../util.h"
#include "../ir/ir_instruction.h"

class AArch64CodeGenerator {
public:
  AArch64CodeGenerator(Logger* logger, TargetOS target);
  std::string generate(const std::vector<IRInstruction>& instructions,
                       bool is_main_defined);

private:
  Logger* m_logger;
  TargetOS m_target;
};

#endif // CODEGEN_AARCH64_BACKEND_H
