#include "backend.h"
#include "runtime_raw.h"

#include "../../parser/types.h"
#include "../../util.h"

static size_t get_align(size_t s) { return ((s + 15) & ~15); }
// static bool is_imm(const IROperand& op) { return std::holds_alternative<IR_Immediate>(op); }

AArch64CodeGenerator::AArch64CodeGenerator(Logger* logger, TargetOS target, bool track_memory, bool freestanding)
    : m_logger(logger),
      m_target(target),
      m_track_memory(track_memory),
      m_freestanding(freestanding),
      m_global_var_alloc(0),
      m_string_count(0) {
  _assert(Type::PTR_SIZE == 8, "ptr size is expected to be 8 bytes for aarch64");

  // Conventions:
  // - callee-saved: sp, x19-x28, x29 (fp), x30 (lr)
  // - caller-saved: x0-x15, x16 (ip0), x17 (ip1)
  //   * arguments: x0-x7 (integers/pointers), v0-v7 (floats/vectors)
  // - return values: x0 (and x1 for 128-bit), v0 (floats)
  // - sp must be 16-byte aligned at all call boundaries
  // - syscalls clobber x0-x7, x16/x17 (Darwin) or x8 (Linux) for syscall number

  m_temp_regs = {"x9", "x10", "x11", "x12", "x13", "x14", "x15"};
  m_callee_saved_regs = {"x19", "x20", "x21", "x22", "x23", "x24", "x25", "x26", "x27", "x28"};
  m_caller_saved_regs = {"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15"};
  m_arg_regs = {"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"};
}

std::string AArch64CodeGenerator::generate(const std::vector<IRInstruction>& instructions, bool is_main_defined) {
  return generate_assembly(instructions, is_main_defined);
}

void AArch64CodeGenerator::clear_func_data() {
  // clear all old local data to the previous function
  m_ctx = FunctionContext();
  m_call_stack = std::stack<CallContext>();
}

std::string AArch64CodeGenerator::get_temp_phys_reg() {
  std::string reg = m_temp_regs[m_ctx.reg_count];
  m_ctx.reg_count = (m_ctx.reg_count + 1) % m_temp_regs.size();

  // reg currently holds a valid IR register -> spill that IR register
  auto itr = m_ctx.phys_reg_to_ir_reg.find(reg);
  if (itr != m_ctx.phys_reg_to_ir_reg.end() && itr->second.first != -1) {
    spill_register(reg, itr->second.first, itr->second.second);
  }

  // mark this one as scratch register
  m_ctx.phys_reg_to_ir_reg[reg] = std::make_pair(-1, 0);
  return reg;
}

std::string AArch64CodeGenerator::get_phys_reg(const IR_Register& ir_reg) {
  // check if already allocated
  auto it_phys = m_ctx.ir_reg_to_phys_reg.find(ir_reg.id);
  if (it_phys != m_ctx.ir_reg_to_phys_reg.end()) {
    return it_phys->second;
  }

  // round-robin selection
  std::string reg = m_temp_regs[m_ctx.reg_count];
  m_ctx.reg_count = (m_ctx.reg_count + 1) % m_temp_regs.size();

  // check if we need to spill register
  auto it_pir = m_ctx.phys_reg_to_ir_reg.find(reg);
  if (it_pir != m_ctx.phys_reg_to_ir_reg.end() && it_pir->second.first != -1) {
    spill_register(reg, it_pir->second.first, it_pir->second.second);
  }

  // if the IR register was previously spilled, load it back from the stack
  auto spill_itr = m_ctx.spilled_ir_reg_locations.find(ir_reg.id);
  if (spill_itr != m_ctx.spilled_ir_reg_locations.end()) {
    size_t offset = spill_itr->second.first;
    std::uint64_t size = spill_itr->second.second;
    std::string offset_reg = "x16"; // don't use get_temp_phys_reg() so that we don't cycle the round robin
    emit("ldr " + offset_reg + ", =" + std::to_string(offset));
    emit("sub " + offset_reg + ", x29, " + offset_reg);
    emit(get_ldr_instr(reg, offset_reg, size) + " // loading back from spill");
    m_ctx.spilled_ir_reg_locations.erase(ir_reg.id);
  }

  m_ctx.ir_reg_to_phys_reg[ir_reg.id] = reg;
  m_ctx.phys_reg_to_ir_reg[reg] = std::make_pair(ir_reg.id, Type::PTR_SIZE);

  return reg;
}

void AArch64CodeGenerator::spill_register(const std::string& reg, int ir_reg, std::uint64_t old_reg_size) {
  m_ctx.stack_offset += old_reg_size;
  size_t offset = m_ctx.stack_offset;

  std::string sized_reg = get_sized_reg(reg, old_reg_size);
  std::string offset_reg = "x16"; // don't use get_temp_phys_reg() so that we don't cycle the round robin
  emit("ldr " + offset_reg + ", =" + std::to_string(offset));
  emit("sub " + offset_reg + ", x29, " + offset_reg);
  emit(get_str_instr(sized_reg, offset_reg, old_reg_size) + " // spilling register " + sized_reg + " to stack");

  m_ctx.spilled_ir_reg_locations[ir_reg] = std::make_pair(offset, old_reg_size);
  m_ctx.ir_reg_to_phys_reg.erase(ir_reg);
  m_ctx.phys_reg_to_ir_reg[reg] = std::make_pair(-1, 0); // mark as free
}

void AArch64CodeGenerator::emit_runtime_call(const std::string& func_name, const std::vector<std::string>& arg_setup_instrs) {
  save_caller_saved_regs();

  for (const std::string& instr : arg_setup_instrs) emit(instr);
  emit("bl " + func_name);
}

void AArch64CodeGenerator::save_caller_saved_regs() {
  emit("// spilling all caller-saved registers that we're using");
  std::vector<std::string> regs_to_save;
  for (const std::string& reg : m_caller_saved_regs) {
    auto it = m_ctx.phys_reg_to_ir_reg.find(reg);
    if (it != m_ctx.phys_reg_to_ir_reg.end() && it->second.first != -1) {
      regs_to_save.push_back(reg);
    }
  }
  for (const std::string& reg : regs_to_save) {
    auto it = m_ctx.phys_reg_to_ir_reg.find(reg);
    if (it != m_ctx.phys_reg_to_ir_reg.end() && it->second.first != -1) {
      spill_register(reg, it->second.first, it->second.second);
    }
  }
}

std::vector<std::string> AArch64CodeGenerator::get_used_callee_regs() {
  std::vector<std::string> used;
  for (const std::string& reg : m_callee_saved_regs) {
    auto it = m_ctx.phys_reg_to_ir_reg.find(reg);
    if (it != m_ctx.phys_reg_to_ir_reg.end() && it->second.first != -1) {
      used.push_back(reg);
    }
  }
  if (used.size() % 2 != 0) used.push_back("xzr");
  return used;
}

std::string AArch64CodeGenerator::emit_get_var_addr(const IR_Variable& ir_var) {
  std::string addr_reg = get_temp_phys_reg();

  if (ir_var.is_global) {
    auto itr = m_glob_var_locations.find(ir_var.name);
    if (itr == m_glob_var_locations.end()) {
      // lazy allocation for globals
      size_t align = ir_var.alignment > 0 ? ir_var.alignment : 1;
      m_global_var_alloc = (m_global_var_alloc + align - 1) & ~(align - 1);
      itr = m_glob_var_locations.insert({ir_var.name, m_global_var_alloc}).first;
      m_global_var_alloc += ir_var.size;
    }

    emit("LOAD_ADDR " + addr_reg + ", global_vars");

    size_t offset = itr->second;
    if (offset > 0) {
      if (offset < 4096) { // immediate add optimization
        emit("add " + addr_reg + ", " + addr_reg + ", #" + std::to_string(offset));
      } else {
        std::string offset_reg = "x16"; // same as other reasons for not using get_temp_phys_reg
        emit("ldr " + offset_reg + ", =" + std::to_string(offset));
        emit("add " + addr_reg + ", " + addr_reg + ", " + offset_reg);
      }
    }
  } else {
    auto itr = m_ctx.var_locations.find(ir_var.name);
    if (itr == m_ctx.var_locations.end()) {
      // lazy allocation for locals
      size_t align = ir_var.alignment > 0 ? ir_var.alignment : 1;
      m_ctx.stack_offset = (m_ctx.stack_offset + align - 1) & ~(align - 1);
      m_ctx.stack_offset += ir_var.size;
      itr = m_ctx.var_locations.insert({ir_var.name, m_ctx.stack_offset}).first;
    }
    emit("ldr " + addr_reg + ", =" + std::to_string(itr->second));
    emit("sub " + addr_reg + ", x29, " + addr_reg);
  }

  return addr_reg;
}

void AArch64CodeGenerator::load_into_reg(const IROperand& operand, const std::string& dst_reg, std::uint64_t size) {
  std::string dst_reg_64 = get_sized_reg(dst_reg, 8);

  if (std::holds_alternative<IR_Register>(operand)) {
    std::string src_reg = get_phys_reg(std::get<IR_Register>(operand));
    if (dst_reg != src_reg) {
      emit("mov " + get_sized_reg(dst_reg, size) + ", " + get_sized_reg(src_reg, size));
    }
  } else if (std::holds_alternative<IR_Variable>(operand)) {
    const IR_Variable& ir_var = std::get<IR_Variable>(operand);
    std::string name = ir_var.name;
    if (ir_var.is_func_decl) {
      // find the underscore before the scope in the IR variable
      size_t us_pos = name.rfind('_');
      _assert(us_pos != std::string::npos, "underscore should exist in func name");
      name = name.substr(0, us_pos);
      if (name != "main" && !ir_var.is_extern) name = "_" + name;
      emit("LOAD_ADDR " + dst_reg_64 + ", " + name);
    } else {
      std::string addr_reg = emit_get_var_addr(ir_var);
      emit(get_ldr_instr(dst_reg, addr_reg, size));
    }
  } else if (std::holds_alternative<IR_ParameterSlot>(operand)) {
    const IR_ParameterSlot& slot = std::get<IR_ParameterSlot>(operand);
    if (slot.is_hidden_ret_ptr) {
      m_ctx.has_hidden_arg = true;
      emit("mov " + get_sized_reg(dst_reg, size) + ", " + get_sized_reg(m_arg_regs[0], size));
    } else {
      size_t physical_idx = slot.index + (m_ctx.has_hidden_arg ? 1 : 0);
      if (physical_idx < 8) {
        emit("mov " + get_sized_reg(dst_reg, size) + ", " + get_sized_reg(m_arg_regs[physical_idx], size));
      } else {
        // remaining args passed on the stack above the return address
        // Stack frame: [rbp] is old rbp, [rbp+8] is return addr, [rbp+16] is 9th arg
        size_t stack_offset = 16 + ((physical_idx - 8) * 8);
        std::string offset_reg = get_temp_phys_reg();

        emit("ldr " + offset_reg + ", =" + std::to_string(stack_offset));
        emit("add " + offset_reg + ", x29, " + offset_reg);
        emit(get_ldr_instr(dst_reg, offset_reg, size));
      }
    }
  } else if (std::holds_alternative<IR_Immediate>(operand)) {
    emit("ldr " + dst_reg_64 + ", =" + std::to_string(std::get<IR_Immediate>(operand).val));
  } else if (std::holds_alternative<IR_Label>(operand)) {
    emit("LOAD_ADDR " + dst_reg_64 + ", " + std::get<IR_Label>(operand).name);
  } else if (std::holds_alternative<std::string>(operand)) {
    const std::string& str_val = std::get<std::string>(operand);
    auto itr = m_string_literal_to_label.find(str_val);
    std::string label;
    if (itr == m_string_literal_to_label.end()) {
      label = "LC" + std::to_string(m_string_count++);
      m_string_literal_to_label.insert({str_val, label});
      m_string_literals_data.push_back(str_val);
    } else {
      label = itr->second;
    }
    emit("LOAD_ADDR " + dst_reg_64 + ", " + label);
  }
}

// performs the round-robin for temp register
std::string AArch64CodeGenerator::load_to_reg(const IROperand& operand, std::uint64_t size) {
  std::string dst_reg = get_temp_phys_reg();
  load_into_reg(operand, dst_reg, size);
  return dst_reg;
}

void AArch64CodeGenerator::store_to_operand(const IROperand& dst, const std::string& src_reg, std::uint64_t size) {
  if (std::holds_alternative<IR_Register>(dst)) {
    std::string dst_reg = get_phys_reg(std::get<IR_Register>(dst));
    emit("mov " + get_sized_reg(dst_reg, size) + ", " + get_sized_reg(src_reg, size));
  } else if (std::holds_alternative<IR_Variable>(dst)) {
    const IR_Variable& ir_var = std::get<IR_Variable>(dst);
    std::string addr_reg = emit_get_var_addr(ir_var);
    emit(get_str_instr(src_reg, addr_reg, size));
  } else {
    _assert(false, "Cannot store to this operand type directly");
  }
}

std::string AArch64CodeGenerator::get_sized_reg(const std::string& reg_name, std::uint64_t size) {
  if (reg_name == "xzr" || reg_name == "wzr") return size == 8 ? "xzr" : "wzr";

  if (reg_name[0] == 'x' || reg_name[0] == 'w') {
    return (size == 8 ? "x" : "w") + reg_name.substr(1);
  }

  return reg_name;
}

std::string AArch64CodeGenerator::get_ldr_instr(const std::string& reg, const std::string& addr_reg, std::uint64_t size) {
  if (size == 8) return "ldr " + get_sized_reg(reg, 8) + ", [" + addr_reg + "]";
  if (size == 4) return "ldr " + get_sized_reg(reg, 4) + ", [" + addr_reg + "]";
  if (size == 2) return "ldrh " + get_sized_reg(reg, 4) + ", [" + addr_reg + "]";
  if (size == 1) return "ldrb " + get_sized_reg(reg, 4) + ", [" + addr_reg + "]";
  _assert(false, "Unsupported load size");
}

std::string AArch64CodeGenerator::get_str_instr(const std::string& reg, const std::string& addr_reg, std::uint64_t size) {
  if (size == 8) return "str " + get_sized_reg(reg, 8) + ", [" + addr_reg + "]";
  if (size == 4) return "str " + get_sized_reg(reg, 4) + ", [" + addr_reg + "]";
  if (size == 2) return "strh " + get_sized_reg(reg, 4) + ", [" + addr_reg + "]";
  if (size == 1) return "strb " + get_sized_reg(reg, 4) + ", [" + addr_reg + "]";
  _assert(false, "Unsupported store size");
}

void AArch64CodeGenerator::emit(const std::string& instruction) {
  if (!m_call_stack.empty()) {
    m_call_stack.top().arg_instrs.push_back(instruction);
  } else if (m_ctx.is_buffering) {
    m_ctx.asm_buffer.push_back("\t" + instruction);
  } else {
    m_out << "\t" << instruction << "\n";
  }
}

void AArch64CodeGenerator::emit_label(const std::string& label_name) {
  if (m_ctx.is_buffering) {
    m_ctx.asm_buffer.push_back(label_name + ":");
  } else {
    m_out << label_name << ":\n";
  }
}

std::string AArch64CodeGenerator::generate_assembly(const std::vector<IRInstruction>& instructions, bool is_main_defined) {
  clear_func_data();
  m_string_literals_data.clear();
  m_string_literal_to_label.clear();

  std::vector<IRInstruction> top_level;
  std::vector<IRInstruction> functions;
  bool in_func = false;
  for (const IRInstruction& instr : instructions) {
    if (instr.opcode == IROpCode::BEGIN_FUNC) {
      in_func = true;
      functions.push_back(instr);
    } else if (instr.opcode == IROpCode::END_FUNC) {
      functions.push_back(instr);
      in_func = false;
    } else if (in_func) {
      functions.push_back(instr);
    } else {
      top_level.push_back(instr);
    }
  }

  emit_program_header();

  // only process start function if we are not freestanding.
  // note that we also won't process global instructions in freestanding mode.
  if (!m_freestanding) {
    handle_begin_func(IRInstruction(IROpCode::BEGIN_FUNC, IR_Label("_start"), {}));

    if (m_target == TargetOS::MacOS) {
      emit("str x0, [x29, #-8]  // save argc");
      emit("str x1, [x29, #-16] // save argv");
    } else {
      emit("ldr x0, [x29, #16]  // grab argc");
      emit("str x0, [x29, #-8]  // save argc");
      emit("add x1, x29, #24    // grab address of argv");
      emit("str x1, [x29, #-16] // save argv");
    }

    m_ctx.stack_offset += 16;
    // emit the top level instructions only, within _start procedure
    for (const IRInstruction& instr : top_level) {
      handle_instruction(instr);
    }

    if (is_main_defined) {
      emit("ldr x0, [x29, #-8]  // restore argc");
      emit("ldr x1, [x29, #-16] // restore argv");
      emit("bl main");
      emit("and x0, x0, #0xFF // enforce 8-bit POSIX exit code truncation");
    } else {
      emit("mov x0, #0 // default exit code");
    }

    handle_end_func(true);
  }

  // process all functions underneath the top level instructions
  for (const auto& instr : functions) {
    handle_instruction(instr);
  }

  emit_program_footer();
  return m_out.str();
}

void AArch64CodeGenerator::emit_program_header() {
  m_out << "  .align 2\n\n";

  // if we are freestanding, we don't want to inject the
  //  runtime ASM, or specify any sections
  if (m_freestanding) return;

  m_out << "  .global _start\n";

  /* insert asm runtime library */
  if (m_target == TargetOS::MacOS) {
    m_out << ".set TARGET_MACOS, 1\n";
  } else if (m_target == TargetOS::Linux) {
    m_out << ".set TARGET_LINUX, 1\n";
  }
  if (m_track_memory) {
    m_out << ".set TRACK_MEMORY, 1\n\n";
  }
  m_out << RUNTIME_ASM << "\n";

  m_out << "  .text\n";
  // _start can now begin emitting here
}

void AArch64CodeGenerator::emit_program_footer() {
  if (!m_string_literals_data.empty()) {
    if (!m_freestanding) {
      m_out << "\n  .data\n";
    }
    for (const std::string& str : m_string_literals_data) {
      m_out << m_string_literal_to_label.at(str) << ":\n";
      m_out << "  .asciz \"";
      for (char c : str) {
        if (c == '"') m_out << "\\\"";
        else if (c == '\n') m_out << "\\n";
        else if (c == '\t') m_out << "\\t";
        else if (c == '\\') m_out << "\\\\";
        else m_out << c;
      }
      m_out << "\"\n";
    }
  }

  if (m_global_var_alloc > 0) {
    if (!m_freestanding) {
      m_out << "  .bss\n";
    }
    m_out << "  .align 3\n";
    m_out << "global_vars: .space " << std::to_string(get_align(m_global_var_alloc)) << "\n";
  }
}

void AArch64CodeGenerator::handle_instruction(const IRInstruction& instr) {
  switch (instr.opcode) {
    case IROpCode::BEGIN_FUNC: handle_begin_func(instr); break;
    case IROpCode::RETURN: handle_return(instr); break;
    case IROpCode::END_FUNC: handle_end_func(false); break;
    case IROpCode::EXIT: handle_exit(instr); break;
    case IROpCode::ASSIGN: handle_assign(instr); break;
    case IROpCode::LOAD: handle_load(instr); break;
    case IROpCode::STORE: handle_store(instr); break;
    case IROpCode::ADD: handle_prim_binop("add", instr); break;
    case IROpCode::SUB: handle_prim_binop("sub", instr); break;
    case IROpCode::MUL: handle_prim_binop("mul", instr); break;
    case IROpCode::DIV: handle_div(instr); break;
    case IROpCode::MOD: handle_mod(instr); break;
    case IROpCode::NEG: handle_neg(instr); break;
    case IROpCode::CMP_EQ:
    case IROpCode::CMP_NE:
    case IROpCode::CMP_LT:
    case IROpCode::CMP_LE:
    case IROpCode::CMP_GT:
    case IROpCode::CMP_GE: handle_cmp(instr); break;
    case IROpCode::NOT: handle_logical_not(instr); break;
    case IROpCode::AND: handle_prim_binop("and", instr); break;
    case IROpCode::OR: handle_prim_binop("orr", instr); break;
    case IROpCode::BIT_AND: handle_prim_binop("and", instr); break;
    case IROpCode::BIT_OR: handle_prim_binop("orr", instr); break;
    case IROpCode::BIT_XOR: handle_prim_binop("eor", instr); break;
    case IROpCode::SHL: handle_shift("lsl", instr); break;
    case IROpCode::SHR: handle_shift("asr", instr); break;
    case IROpCode::LABEL: handle_label(instr); break;
    case IROpCode::GOTO: handle_goto(instr); break;
    case IROpCode::IF_Z: handle_if_z(instr); break;
    case IROpCode::BEGIN_LCALL_PREP: m_call_stack.push(CallContext{}); break;
    case IROpCode::SET_HIDDEN_ARG: handle_set_hidden_arg(instr); break;
    case IROpCode::PUSH_ARG: handle_push_arg(instr); break;
    case IROpCode::LCALL: handle_lcall(instr); break;
    case IROpCode::ASM_BLOCK: handle_asm_block(instr); break;
    case IROpCode::ADDR_OF: handle_addr_of(instr); break;
    case IROpCode::ALLOC: handle_alloc(instr); break;
    case IROpCode::ALLOC_ARRAY: handle_alloc_array(instr); break;
    case IROpCode::FREE: handle_free(instr); break;
    case IROpCode::MEM_COPY: handle_mem_copy(instr); break;
    case IROpCode::MEM_SET: handle_mem_set(instr); break;
    default:
      _assert(false, "Unsupported IR opcode for aarch64");
  }
}

void AArch64CodeGenerator::handle_begin_func(const IRInstruction& instr) {
  clear_func_data();
  const IROperand& label = instr.result.value();
  emit_label(std::get<IR_Label>(label).name);

  emit("stp x29, x30, [sp, #-16]!");
  emit("mov x29, sp");

  m_ctx.alloc_placeholder_idx = m_ctx.asm_buffer.size();
  m_ctx.is_buffering = true;

  m_ctx.asm_buffer.push_back("");
  m_ctx.asm_buffer.push_back("");
}

void AArch64CodeGenerator::handle_end_preamble() {
  size_t total_stack = get_align(m_ctx.stack_offset);

  // perform function post processing
  size_t idx = m_ctx.alloc_placeholder_idx;
  if (idx < m_ctx.asm_buffer.size() && total_stack > 0) {
    m_ctx.asm_buffer[idx] = "\tsub sp, sp, #" + std::to_string(total_stack);
  }

  // save callee-saved registers that are in use
  std::string& placeheld = m_ctx.asm_buffer[idx + 1];
  const std::vector<std::string>& used_regs = get_used_callee_regs();
  for (size_t i = 0; i < used_regs.size(); i += 2) {
    if (!placeheld.empty()) placeheld += "\n";
    placeheld += "\tstp " + used_regs[i] + ", " + used_regs[i + 1] + ", [sp, #-16]! // saving callee-saved registers";
  }

  // write the buffered asm contents:
  for (size_t i = 0; i < m_ctx.asm_buffer.size(); ++i) {
    if (i < 2 && m_ctx.asm_buffer[i].empty()) continue;
    m_out << m_ctx.asm_buffer[i] << "\n";
  }
  m_ctx.asm_buffer.clear();
  m_ctx.is_buffering = false;

  // restore callee-saved registers in reverse order
  for (int i = used_regs.size() - 2; i >= 0; i -= 2) {
    emit("ldp " + used_regs[i] + ", " + used_regs[i + 1] + ", [sp], #16 // restoring callee-saved registers");
  }
}

void AArch64CodeGenerator::handle_end_func(bool is_exit) {
  handle_end_preamble();
  emit("mov sp, x29");
  emit("ldp x29, x30, [sp], #16 // restore stack");
  if (is_exit) {
    emit("bl exit");
  } else {
    emit("ret");
  }
}

void AArch64CodeGenerator::handle_exit(const IRInstruction& instr) {
  std::string exit_code = load_to_reg(instr.operands[0], instr.size);
  emit("mov " + get_sized_reg("x0", instr.size) + ", " + get_sized_reg(exit_code, instr.size));
  emit("and x0, x0, #0xFF // enforce 8-bit POSIX exit code truncation");
  emit("bl exit");
}

void AArch64CodeGenerator::handle_return(const IRInstruction& instr) {
  if (!instr.operands.empty()) {
    std::string ret_val = load_to_reg(instr.operands[0], instr.size);
    emit("mov " + get_sized_reg("x0", instr.size) + ", " + get_sized_reg(ret_val, instr.size));
  } else {
    emit("mov x0, #0");
  }
}

void AArch64CodeGenerator::handle_assign(const IRInstruction& instr) {
  IROperand dst = instr.result.value();
  IROperand src = instr.operands[0];

  std::uint64_t dst_size = instr.size;
  std::uint64_t src_size = dst_size;
  if (std::holds_alternative<IR_Variable>(src)) src_size = std::get<IR_Variable>(src).size;

  std::string src_reg = load_to_reg(src, src_size);
  store_to_operand(dst, src_reg, dst_size);
}

void AArch64CodeGenerator::handle_prim_binop(const std::string& aarch64_instr, const IRInstruction& instr) {
  std::string dst_reg = get_phys_reg(std::get<IR_Register>(instr.result.value()));
  std::string s_dst = get_sized_reg(dst_reg, instr.size);

  load_into_reg(instr.operands[0], dst_reg, instr.size);
  std::string r2 = load_to_reg(instr.operands[1], instr.size);
  std::string s_r2 = get_sized_reg(r2, instr.size);

  emit(aarch64_instr + " " + s_dst + ", " + s_dst + ", " + s_r2);
}

void AArch64CodeGenerator::handle_div(const IRInstruction& instr) {
  handle_prim_binop("sdiv", instr);
}

void AArch64CodeGenerator::handle_mod(const IRInstruction& instr) {
std::string dst_reg = get_phys_reg(std::get<IR_Register>(instr.result.value()));
  std::string s_dst = get_sized_reg(dst_reg, instr.size);
  load_into_reg(instr.operands[0], dst_reg, instr.size);

  std::string r2 = load_to_reg(instr.operands[1], instr.size);
  std::string s_r2 = get_sized_reg(r2, instr.size);

  std::string tmp = get_temp_phys_reg();
  std::string s_tmp = get_sized_reg(tmp, instr.size);

  emit("sdiv " + s_tmp + ", " + s_dst + ", " + s_r2);
  emit("msub " + s_dst + ", " + s_tmp + ", " + s_r2 + ", " + s_dst);
}

void AArch64CodeGenerator::handle_neg(const IRInstruction& instr) {
  std::string dst_reg = get_phys_reg(std::get<IR_Register>(instr.result.value()));
  std::string src = load_to_reg(instr.operands[0], instr.size);
  emit("neg " + get_sized_reg(dst_reg, instr.size) + ", " + get_sized_reg(src, instr.size));
}

void AArch64CodeGenerator::handle_logical_not(const IRInstruction& instr) {
  std::string dst_reg = get_phys_reg(std::get<IR_Register>(instr.result.value()));
  std::string src = load_to_reg(instr.operands[0], instr.size);
  emit("cmp " + get_sized_reg(src, instr.size) + ", #0");
  emit("cset " + get_sized_reg(dst_reg, 8) + ", eq");
}

void AArch64CodeGenerator::handle_shift(const std::string& aarch64_instr, const IRInstruction& instr) {
  handle_prim_binop(aarch64_instr, instr);
}

void AArch64CodeGenerator::handle_cmp(const IRInstruction& instr) {
  std::string r1 = load_to_reg(instr.operands[0], instr.size);
  std::string r2 = load_to_reg(instr.operands[1], instr.size);

  std::string dst_reg = get_phys_reg(std::get<IR_Register>(instr.result.value()));

  emit("cmp " + get_sized_reg(r1, instr.size) + ", " + get_sized_reg(r2, instr.size));

  std::string cond;
  switch (instr.opcode) {
    case IROpCode::CMP_EQ: cond = "eq"; break;
    case IROpCode::CMP_NE: cond = "ne"; break;
    case IROpCode::CMP_LT: cond = "lt"; break;
    case IROpCode::CMP_LE: cond = "le"; break;
    case IROpCode::CMP_GT: cond = "gt"; break;
    case IROpCode::CMP_GE: cond = "ge"; break;
    default: _assert(false, "Unhandled CMP type");
  }

  emit("cset " + get_sized_reg(dst_reg, 8) + ", " + cond);
}

void AArch64CodeGenerator::handle_label(const IRInstruction& instr) {
  emit_label(std::get<IR_Label>(instr.result.value()).name);
}

void AArch64CodeGenerator::handle_goto(const IRInstruction& instr) {
  emit("b " + std::get<IR_Label>(instr.operands[0]).name);
}

void AArch64CodeGenerator::handle_if_z(const IRInstruction& instr) {
  std::string cond = load_to_reg(instr.operands[0], instr.size);
  emit("cbz " + get_sized_reg(cond, instr.size) + ", " + std::get<IR_Label>(instr.operands[1]).name);
}

void AArch64CodeGenerator::handle_set_hidden_arg(const IRInstruction& instr) {
  CallContext& ctx = m_call_stack.top();
  std::string src = load_to_reg(instr.operands[0], Type::PTR_SIZE);
  ctx.arg_instrs.push_back("mov x0, " + get_sized_reg(src, 8));
  ctx.has_hidden_arg = true;
}

void AArch64CodeGenerator::handle_push_arg(const IRInstruction& instr) {
  CallContext& ctx = m_call_stack.top();
  std::string src = load_to_reg(instr.operands[0], instr.size);

  size_t physical_idx = ctx.current_args_passed + (ctx.has_hidden_arg ? 1 : 0);
  if (physical_idx < 8) {
    ctx.arg_instrs.push_back("mov " + get_sized_reg(m_arg_regs[physical_idx], instr.size) + ", " + get_sized_reg(src, instr.size));
  } else {
    ctx.arg_instrs.push_back("str " + get_sized_reg(src, 8) + ", [sp, #" + std::to_string(ctx.stack_args_size) + "]");
    ctx.stack_args_size += 8;
  }
  ctx.current_args_passed++;
}

void AArch64CodeGenerator::handle_lcall(const IRInstruction& instr) {
  save_caller_saved_regs();
  CallContext ctx = m_call_stack.top();
  m_call_stack.pop();

  size_t pad = (ctx.stack_args_size % 16 != 0) ? 8 : 0;
  size_t total_stack = ctx.stack_args_size + pad;

  if (total_stack > 0) emit("sub sp, sp, #" + std::to_string(total_stack));
  for (const std::string& arg_instr : ctx.arg_instrs) emit(arg_instr);

  const IROperand& label = instr.operands[0];
  if (std::holds_alternative<IR_Label>(label)) {
    emit("bl " + std::get<IR_Label>(label).name);
  } else {
    std::string ptr = load_to_reg(label, Type::PTR_SIZE);
    emit("blr " + ptr);
  }

  if (total_stack > 0) emit("add sp, sp, #" + std::to_string(total_stack));

  if (instr.result.has_value()) {
    std::string dst = load_to_reg(instr.result.value(), instr.size);
    emit("mov " + get_sized_reg(dst, instr.size) + ", " + get_sized_reg("x0", instr.size));
    store_to_operand(instr.result.value(), dst, instr.size);
  }
}

void AArch64CodeGenerator::handle_asm_block(const IRInstruction& instr) {
  emit(std::get<std::string>(instr.operands[0]));
}

void AArch64CodeGenerator::handle_load(const IRInstruction& instr) {
  std::string addr_reg = load_to_reg(instr.operands[0], Type::PTR_SIZE);
  std::string dst = load_to_reg(instr.result.value(), instr.size);
  emit(get_ldr_instr(dst, addr_reg, instr.size));
  store_to_operand(instr.result.value(), dst, instr.size);
}

void AArch64CodeGenerator::handle_store(const IRInstruction& instr) {
  std::string addr_reg = load_to_reg(instr.result.value(), Type::PTR_SIZE);
  std::string val_reg = load_to_reg(instr.operands[0], instr.size);
  emit(get_str_instr(val_reg, addr_reg, instr.size));
}

void AArch64CodeGenerator::handle_addr_of(const IRInstruction& instr) {
  std::string dst = load_to_reg(instr.result.value(), Type::PTR_SIZE);
  const IROperand& src = instr.operands[0];

  if (std::holds_alternative<IR_Variable>(src)) {
    const IR_Variable& ir_var = std::get<IR_Variable>(src);
    std::string addr = emit_get_var_addr(ir_var);
    emit("mov " + get_sized_reg(dst, 8) + ", " + addr);
  }
  store_to_operand(instr.result.value(), dst, Type::PTR_SIZE);
}

void AArch64CodeGenerator::handle_alloc(const IRInstruction& instr) {
  std::uint64_t type_size = std::get<IR_Immediate>(instr.operands[0]).val;
  emit_runtime_call("malloc", {"ldr x0, =" + std::to_string(type_size)});

  std::string dst = load_to_reg(instr.result.value(), Type::PTR_SIZE);
  emit("mov " + get_sized_reg(dst, 8) + ", x0");

  if (instr.operands.size() > 1) {
    std::string init_val = load_to_reg(instr.operands[1], type_size);
    emit(get_str_instr(init_val, dst, type_size));
  }
  store_to_operand(instr.result.value(), dst, Type::PTR_SIZE);
}

void AArch64CodeGenerator::handle_alloc_array(const IRInstruction& instr) {
  std::uint64_t el_size = std::get<IR_Immediate>(instr.operands[0]).val;
  std::string count = load_to_reg(instr.operands[1], instr.size);
  std::string tmp = get_temp_phys_reg();

  emit("ldr " + tmp + ", =" + std::to_string(el_size));
  emit("mul " + tmp + ", " + get_sized_reg(count, 8) + ", " + tmp);
  emit_runtime_call("malloc", {"mov x0, " + tmp});

  std::string dst = load_to_reg(instr.result.value(), Type::PTR_SIZE);
  emit("mov " + get_sized_reg(dst, 8) + ", x0");
  store_to_operand(instr.result.value(), dst, Type::PTR_SIZE);
}

void AArch64CodeGenerator::handle_free(const IRInstruction& instr) {
  std::string ptr = load_to_reg(instr.operands[0], Type::PTR_SIZE);
  emit_runtime_call("free", {"mov x0, " + get_sized_reg(ptr, 8)});
  emit("mov " + get_sized_reg(ptr, 8) + ", #0");
  store_to_operand(instr.operands[0], ptr, Type::PTR_SIZE);
}

void AArch64CodeGenerator::handle_mem_copy(const IRInstruction& instr) {
  std::string dst = load_to_reg(instr.result.value(), Type::PTR_SIZE);
  std::string src = load_to_reg(instr.operands[0], Type::PTR_SIZE);

  emit_runtime_call("memcpy", {
      "mov x0, " + get_sized_reg(dst, 8),
      "mov x1, " + get_sized_reg(src, 8),
      "ldr x2, =" + std::to_string(instr.size)
  });
}

void AArch64CodeGenerator::handle_mem_set(const IRInstruction& instr) {
  std::string dst = load_to_reg(instr.result.value(), Type::PTR_SIZE);
  std::string val = load_to_reg(instr.operands[0], 1);

  emit_runtime_call("memset", {
      "mov x0, " + get_sized_reg(dst, 8),
      "mov x1, " + get_sized_reg(val, 8),
      "ldr x2, =" + std::to_string(instr.size)
  });
}
