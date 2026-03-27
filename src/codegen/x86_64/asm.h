#ifndef CODEGEN_X86_64_ASM_H
#define CODEGEN_X86_64_ASM_H

#include <iostream>
#include <list>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../logging/logger.h"
#include "../ir/ir_instruction.h"

struct CallContext {
  size_t stack_args_size = 0;
  size_t current_args_passed = 0;
  std::vector<std::string> arg_instrs;
  std::vector<std::string> used_caller_saved;
};

class X86_64CodeGenerator {
public:
  X86_64CodeGenerator(Logger* logger);

  std::string generate(const std::vector<IRInstruction>& instructions,
                       bool is_main_defined);

private:
  Logger* m_logger;
  std::ostringstream m_out;

  bool m_handling_top_level;
  size_t m_global_var_alloc;
  std::unordered_map<std::string, std::string> m_var_locations; // in stack
  std::unordered_map<std::string, std::string> m_glob_var_locations;

  bool m_is_buffering_function;
  std::vector<std::string> m_current_func_asm_buffer;
  size_t m_current_func_alloc_placeholder_idx;
  size_t m_current_func_stack_offset;

  // argument handling
  std::vector<std::string> m_arg_regs;           // register argument order

  std::vector<std::string> m_string_literals_data;
  std::unordered_map<std::string, std::string> m_string_literal_to_label;
  size_t m_string_count;

  // register allocation
  // {x86_reg_str, <IR_reg_id, reg_size>}
  std::unordered_map<std::string, std::pair<int, std::uint64_t>> m_x86_reg_to_ir_reg;
  std::unordered_map<int, std::string> m_ir_reg_to_x86_reg;
  std::unordered_map<int, std::pair<std::string, std::uint64_t>>
      m_spilled_ir_reg_locations;
  size_t m_reg_count;

  std::vector<std::string> m_temp_regs;
  std::vector<std::string> m_callee_saved_regs;
  std::vector<std::string> m_caller_saved_regs;

  std::stack<CallContext> m_call_stack;

  void save_caller_saved_regs();
  void restore_caller_saved_regs(const std::vector<std::string>& used_caller_saved);
  std::vector<std::string> get_used_callee_regs();

  void clear_func_data();

  std::string get_size_prefix(std::uint64_t size);
  std::string get_sized_register_name(const std::string& reg64_name,
                                      std::uint64_t size);
  std::string operand_to_string(const IROperand& operand);
  std::string get_sized_component(const IROperand& operand, std::uint64_t size);

  void spill_register(const std::string& reg, int ir_reg,
                      std::uint64_t old_reg_size);
  std::string get_temp_x86_reg(std::uint64_t size);
  std::string get_x86_reg(const IR_Register& ir_reg);
  std::string get_mov_instr(const std::string& reg_64, const std::string& src,
                            bool is_src_immediate, std::uint64_t src_size);
  void emit_one_operand_memory_operation(const IROperand& s1,
                                         const IROperand& s2,
                                         const std::string& operation,
                                         std::uint64_t size);
  void emit_one_operand_memory_operation(const std::string& s1_str,
                                         const std::string& s2_str,
                                         const std::string& operation,
                                         std::uint64_t size);
  void emit(const std::string& instruction);
  void emit_label(const std::string& label_name);

  std::string generate_assembly(const std::vector<IRInstruction>& instructions,
                                bool is_main_defined);
  void handle_instruction(const IRInstruction& instr);

  void handle_begin_func(const IRInstruction& instr);
  void handle_end_preamble();
  void handle_end_func(bool is_exit);
  void handle_return(const IRInstruction& instr);
  void handle_exit(const IRInstruction& instr);

  void handle_assign(const IRInstruction& instr);
  void handle_load(const IRInstruction& instr);
  void handle_store(const IRInstruction& instr);

  void handle_prim_binop(const std::string& x86_instr,
                         const IRInstruction& instr);
  void handle_div(const IRInstruction& instr);
  void handle_mod(const IRInstruction& instr);
  void handle_neg(const IRInstruction& instr);
  void handle_logical_not(const IRInstruction& instr);
  void handle_logical_and_or(const IRInstruction& instr,
                             const std::string& op_mnemonic);
  void handle_shift(const std::string& x86_instr, const IRInstruction& instr);

  void handle_cmp(const IRInstruction& instr); // For all CMP_XX

  void handle_label(const IRInstruction& instr);
  void handle_goto(const IRInstruction& instr);
  void handle_if_z(const IRInstruction& instr);

  void handle_push_arg(const IRInstruction& instr);
  void handle_lcall(const IRInstruction& instr);
  void handle_asm_block(const IRInstruction& instr);

  void handle_addr_of(const IRInstruction& instr);
  void handle_alloc(const IRInstruction& instr);
  void handle_alloc_array(const IRInstruction& instr);
  void handle_free(const IRInstruction& instr);
  void handle_mem_copy(const IRInstruction& instr);
  void handle_mem_set(const IRInstruction& instr);
};

#endif // CODEGEN_X86_64_ASM_H
