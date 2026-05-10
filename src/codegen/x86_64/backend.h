#ifndef CODEGEN_X86_64_BACKEND_H
#define CODEGEN_X86_64_BACKEND_H

#include <iostream>
#include <list>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../util.h"
#include "../../logging/logger.h"
#include "../ir/ir_instruction.h"
#include "../defs.h"

struct X86Operand {
  std::string str;
  bool is_mem;
  bool is_imm;
};

class X86_64CodeGenerator {
public:
  X86_64CodeGenerator(Logger* logger, TargetOS target, bool track_memory, bool freestanding);

  std::string generate(const std::vector<IRInstruction>& instructions, bool is_main_defined);

private:
  Logger* m_logger;
  TargetOS m_target;
  bool m_track_memory;
  bool m_freestanding;
  std::ostringstream m_out;

  /* global state (.data, .bss) */
  size_t m_global_var_alloc;
  std::unordered_map<std::string, size_t> m_glob_var_locations;
  size_t m_string_count;
  std::vector<std::string> m_string_literals_data;
  std::unordered_map<std::string, std::string> m_string_literal_to_label;

  /* current procedure state */
  FunctionContext m_ctx;
  void clear_func_data();

  /* register allocation and spilling */
  std::vector<std::string> m_temp_regs;
  std::vector<std::string> m_callee_saved_regs;
  std::vector<std::string> m_caller_saved_regs;
  std::vector<std::string> m_arg_regs; // register argument order
  std::stack<CallContext> m_call_stack;

  std::string get_temp_phys_reg(std::uint64_t size);
  std::string get_phys_reg(const IR_Register& ir_reg);
  void spill_register(const std::string& reg, int ir_reg, std::uint64_t old_reg_size);

  /* proceduring calling */
  void emit_runtime_call(const std::string& func_name, const std::vector<std::string>& arg_setup_instrs);
  void save_caller_saved_regs();
  std::vector<std::string> get_used_callee_regs();

  /* operand resolution and formatting */
  std::string resolve_source_operand(const IROperand& src, std::uint64_t size);
  std::string operand_to_string(const IROperand& operand);
  std::string get_sized_component(const IROperand& operand, std::uint64_t size);
  std::string get_sized_register_name(const std::string& reg64_name, std::uint64_t size);
  std::string get_size_prefix(std::uint64_t size);
  X86Operand get_x86_operand(const IROperand& operand, std::uint64_t size);

  /* assembly emission */
  void emit(const std::string& instruction);
  void emit_label(const std::string& label_name);
  std::string get_mov_instr(const std::string& reg_64, const std::string& src,
                            bool is_src_immediate, std::uint64_t src_size);
  void emit_mem_safe_op(const std::string& opcode, const X86Operand& dst,
                        const X86Operand& src, std::uint64_t size);

  std::string generate_assembly(const std::vector<IRInstruction>& instructions, bool is_main_defined);
  void emit_program_header();
  void emit_program_footer();

  /* instruction handlers */
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

  void handle_set_hidden_arg(const IRInstruction& instr);
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

#endif // CODEGEN_X86_64_BACKEND_H
