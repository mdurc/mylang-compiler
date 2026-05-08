#ifndef CODEGEN_DEFS_H
#define CODEGEN_DEFS_H

#include <vector>
#include <string>
#include <unordered_map>

/* current function that is being executed */
struct FunctionContext {
  bool is_buffering = false;
  std::vector<std::string> asm_buffer;
  size_t alloc_placeholder_idx = 0;
  size_t stack_offset = 0;
  bool has_hidden_arg = false;

  std::unordered_map<std::string, size_t> var_locations;
  std::unordered_map<std::string, std::pair<int, std::uint64_t>> phys_reg_to_ir_reg;
  std::unordered_map<int, std::string> ir_reg_to_phys_reg;
  std::unordered_map<int, std::pair<size_t, std::uint64_t>> spilled_ir_reg_locations;
  size_t reg_count = 0;
};

/* tracking state used in a procedure call within a function */
struct CallContext {
  size_t stack_args_size = 0;
  size_t current_args_passed = 0;
  bool has_hidden_arg = false;
  std::vector<std::string> arg_instrs;
  std::vector<std::string> used_caller_saved;
};

#endif // CODEGEN_DEFS_H
