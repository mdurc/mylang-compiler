#include "asm.h"

#include "../../parser/types.h"
#include "../runtime/builtins.h"
#include "../../util.h"

static size_t get_align(size_t s) { return ((s + 15) & ~15); }
static bool is_imm(IROperand op) { return std::holds_alternative<IR_Immediate>(op); }

X86_64CodeGenerator::X86_64CodeGenerator(Logger* logger)
    : m_logger(logger),
      m_handling_top_level(false),
      m_global_var_alloc(0),
      m_is_buffering_function(false),
      m_current_func_alloc_placeholder_idx(0),
      m_current_func_stack_offset(0),
      m_string_count(0),
      m_reg_count(0) {
  _assert(Type::PTR_SIZE == 8, "ptr size is expected to be 8 bytes for x86_64");
  // Conventions:
  // - callee-saved: rsp, rbp, rbx, r12, r13, r14, r15
  // - caller-saved: rax, rcx, rdx, rsi, rdi, r8-11
  // - rax and r11 are not preserved during syscall
  // - Return value stored in rax, rax = 0 in the case of void return type
  // - Arguments: rdi, rsi, rdx, rcx, r8, r9, then on the stack

  m_temp_regs = {"r10", "r11", "r12", "r13", "r14", "r15", "rbx"};
  m_callee_saved_regs = {"rsp", "rbp", "rbx", "r12", "r13", "r14", "r15"};
  m_caller_saved_regs = {"rdi", "rsi", "rdx", "rcx", "r8",
                         "r9",  "r10", "r11"}; // omits 'rax' intentionally
  m_arg_regs = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
}

std::string X86_64CodeGenerator::get_sized_register_name(
    const std::string& reg64_name, std::uint64_t size) {
  if (size == Type::PTR_SIZE) return reg64_name;
  if (reg64_name == "rax")
    return (size == 4 ? "eax" : (size == 2 ? "ax" : "al"));
  if (reg64_name == "rbx")
    return (size == 4 ? "ebx" : (size == 2 ? "bx" : "bl"));
  if (reg64_name == "rcx")
    return (size == 4 ? "ecx" : (size == 2 ? "cx" : "cl"));
  if (reg64_name == "rdx")
    return (size == 4 ? "edx" : (size == 2 ? "dx" : "dl"));
  if (reg64_name == "rsi")
    return (size == 4 ? "esi" : (size == 2 ? "si" : "sil"));
  if (reg64_name == "rdi")
    return (size == 4 ? "edi" : (size == 2 ? "di" : "dil"));
  if (reg64_name == "rbp")
    return (size == 4 ? "ebp" : (size == 2 ? "bp" : "bpl"));
  if (reg64_name == "rsp")
    return (size == 4 ? "esp" : (size == 2 ? "sp" : "spl"));

  if (reg64_name.find('r') == 0 && reg64_name.size() >= 2 &&
      isdigit(reg64_name[1])) {
    if (size == 4) return reg64_name + "d"; // r8d, r10d
    if (size == 2) return reg64_name + "w"; // r8w, r10w
    if (size == 1) return reg64_name + "b"; // r8b, r10b
  }
  _assert(false, "Cannot get sized name for register: " + reg64_name +
                           " with size " + std::to_string(size));
}

std::string X86_64CodeGenerator::get_size_prefix(std::uint64_t size) {
  switch (size) {
    case 1: return "BYTE";
    case 2: return "WORD";
    case 4: return "DWORD";
    case 8: return "QWORD";
    default:
      _assert(false, "Unsupported size for prefix: " + std::to_string(size));
  }
}

std::string X86_64CodeGenerator::operand_to_string(const IROperand& operand) {
  if (std::holds_alternative<IR_Register>(operand)) { // Register
    return get_x86_reg(std::get<IR_Register>(operand));
  }

  if (std::holds_alternative<IR_Variable>(operand)) { // Variable
    const IR_Variable& ir_var = std::get<IR_Variable>(operand);
    const std::string& var_name = ir_var.name;

    if (ir_var.is_func_decl) {
      // find the underscore before the scope in the IR variable
      size_t underscore_pos = var_name.rfind('_');
      _assert(underscore_pos != std::string::npos, "underscore should exist in func name");

      std::string raw_func_name = var_name.substr(0, underscore_pos);
      if (raw_func_name == "main" || ir_var.is_extern) {
        return raw_func_name;
      }
      return "_" + raw_func_name;
    }

    std::uint64_t var_size = ir_var.size;
    auto itr = m_var_locations.find(var_name);
    if (itr == m_var_locations.end()) {
      itr = m_glob_var_locations.find(var_name);
      if (itr != m_glob_var_locations.end()) {
        return itr->second;
      }

      std::string relative_addr;
      if (m_handling_top_level) {
        relative_addr = "[rel global_vars + " + std::to_string(m_global_var_alloc) + "]";
        // the relative addresses start at 'global_vars + 0', then we add
        // the size of what we just reserved, and let the next variable start
        // at the first byte after this reserved space
        m_global_var_alloc += var_size;
        itr = m_glob_var_locations.insert({var_name, relative_addr}).first;
      } else {
        // not found: we should make new space for this variable in the curr func
        m_current_func_stack_offset += var_size;
        relative_addr = "[rbp - " + std::to_string(m_current_func_stack_offset) + "]";
        itr = m_var_locations.insert({var_name, relative_addr}).first;
      }
    }
    return itr->second;
  }

  if (std::holds_alternative<IR_ParameterSlot>(operand)) { // Parameter
    const IR_ParameterSlot& slot = std::get<IR_ParameterSlot>(operand);
    if (slot.index < 6) { // first 6 args passed in registers
      return m_arg_regs[slot.index];
    } else {
      // remaining args passed on the stack above the return address
      // Stack frame: [rbp] is old rbp, [rbp+8] is return addr, [rbp+16] is 7th arg
      //  (assuming 8-byte stack slots)
      size_t stack_offset = 16 + ((slot.index - 6) * 8);
      return "[rbp + " + std::to_string(stack_offset) + "]";
    }
  }

  if (std::holds_alternative<IR_Immediate>(operand)) { // Immediate
    return std::to_string(std::get<IR_Immediate>(operand).val);
  }

  if (std::holds_alternative<IR_Label>(operand)) { // Label
    return std::get<IR_Label>(operand).name;
  }

  if (std::holds_alternative<std::string>(operand)) { // String literal
    const std::string& str_val = std::get<std::string>(operand);
    // check if string has already been added, otherwise add it to string labels
    auto itr = m_string_literal_to_label.find(str_val);
    if (itr == m_string_literal_to_label.end()) {
      std::string label = "LC" + std::to_string(m_string_count++);
      m_string_literal_to_label.insert({str_val, label});
      m_string_literals_data.push_back(str_val);
      return label;
    }
    return itr->second;
  }

  _assert(false, "Unknown IROperand type");
}

std::string X86_64CodeGenerator::get_sized_component(const IROperand& operand, std::uint64_t size) {
  std::string str = operand_to_string(operand);

  if (std::holds_alternative<IR_Register>(operand)) {
    // register, we substitute it with sized version
    return get_sized_register_name(str, size);
  }

  if (std::holds_alternative<IR_Variable>(operand)) {
    // memory (variables on the stack), add size prefix
    const IR_Variable& var = std::get<IR_Variable>(operand);
    if (var.is_func_decl) {
      return str;
    }
    return get_size_prefix(size) + " " + str;
  }

  if (std::holds_alternative<IR_ParameterSlot>(operand)) {
    // parameters can either be in registers or in memory
    const IR_ParameterSlot& slot = std::get<IR_ParameterSlot>(operand);
    if (slot.index < 6) { // register
      return get_sized_register_name(str, size);
    } else { // stack (memory)
      return get_size_prefix(size) + " " + str;
    }
  }

  // immediate or label, which we leave
  return str;
}

std::string X86_64CodeGenerator::get_mov_instr(const std::string& reg_64,
                                               const std::string& src,
                                               bool is_src_immediate,
                                               std::uint64_t src_size) {
  if (is_src_immediate) {
    // automatically performs zero extension if we load at least 32-bit register
    std::uint64_t dest_size = (src_size == 8) ? 8 : 4;
    return "mov " + get_sized_register_name(reg_64, dest_size) + ", " + src;
  }
  if (src_size < 4) {
    // 1/2 byte registers have to be zero-extended explicitly
    return "movzx " + reg_64 + ", " + src;
  }

  // 4 byte sources will be implicitly zero-extended, 8 byte sources is normal
  return "mov " + get_sized_register_name(reg_64, src_size) + ", " + src;
}

void X86_64CodeGenerator::spill_register(const std::string& reg, int ir_reg,
                                         std::uint64_t old_reg_size) {
  m_current_func_stack_offset += old_reg_size;

  std::string sized_reg = get_sized_register_name(reg, old_reg_size);
  std::string spill_addr = "[rbp - " + std::to_string(m_current_func_stack_offset) + "]";

  emit("mov " + get_size_prefix(old_reg_size) + " " + spill_addr + ", " + sized_reg +
       " ; spilling register " + sized_reg + " to stack");

  m_spilled_ir_reg_locations[ir_reg] = std::make_pair(spill_addr, old_reg_size);
  m_ir_reg_to_x86_reg.erase(ir_reg);
  m_x86_reg_to_ir_reg[reg] = std::make_pair(-1, 0); // mark as free
}

std::string X86_64CodeGenerator::get_temp_x86_reg(std::uint64_t size) {
  std::string reg = m_temp_regs[m_reg_count];
  m_reg_count = (m_reg_count + 1) % m_temp_regs.size();

  // reg currently holds a valid IR register -> spill that IR register
  auto itr = m_x86_reg_to_ir_reg.find(reg);
  bool check_spilling =
      itr != m_x86_reg_to_ir_reg.end() && itr->second.first != -1;
  if (check_spilling) {
    spill_register(reg, itr->second.first, itr->second.second);
  }

  // mark this one as scratch register
  m_x86_reg_to_ir_reg[reg] = std::make_pair(-1, 0);

  return get_sized_register_name(reg, size);
}

std::string X86_64CodeGenerator::get_x86_reg(const IR_Register& ir_reg) {
  // check if already allocated
  auto it_phys = m_ir_reg_to_x86_reg.find(ir_reg.id);
  if (it_phys != m_ir_reg_to_x86_reg.end()) {
    return it_phys->second;
  }

  // round-robin selection
  std::string reg = m_temp_regs[m_reg_count];
  m_reg_count = (m_reg_count + 1) % m_temp_regs.size();

  // check if we need to spill register
  auto it_x86 = m_x86_reg_to_ir_reg.find(reg);
  if (it_x86 != m_x86_reg_to_ir_reg.end() && it_x86->second.first != -1) {
    spill_register(reg, it_x86->second.first, it_x86->second.second);
  }

  // if the IR register was previously spilled, load it back from the stack
  auto spill_itr = m_spilled_ir_reg_locations.find(ir_reg.id);
  if (spill_itr != m_spilled_ir_reg_locations.end()) {
    const std::pair<std::string, std::uint64_t>& loc_size = spill_itr->second;
    emit("mov " + get_sized_register_name(reg, loc_size.second) + ", " +
         get_size_prefix(loc_size.second) + " " + loc_size.first +
         " ; loading back from spill");
    m_spilled_ir_reg_locations.erase(ir_reg.id);
  }

  m_ir_reg_to_x86_reg[ir_reg.id] = reg;

  // we can use ptr_size because this reg has never been used before
  // and we can't know what the size is yet, and this function is only ever
  // used within operand_to_string which is intended to not use size contxt
  m_x86_reg_to_ir_reg[reg] = std::make_pair(ir_reg.id, Type::PTR_SIZE);

  return reg;
}

void X86_64CodeGenerator::emit_one_operand_memory_operation(
    const IROperand& s1, const IROperand& s2, const std::string& operation,
    std::uint64_t size) {
  std::string s1_str = get_sized_component(s1, size);
  std::string s2_str = (s1 == s2) ? s1_str : get_sized_component(s2, size);

  // catch all memory operands (variables, stack parameters, spilled registers)
  //  by the ']' check from overloaded function:
  emit_one_operand_memory_operation(s1_str, s2_str, operation, size);
}

void X86_64CodeGenerator::emit_one_operand_memory_operation(
    const std::string& s1_str, const std::string& s2_str,
    const std::string& operation, std::uint64_t size) {
  if (s1_str.back() == ']' && s2_str.back() == ']') {
    // they are both memory, so put the second one in a register
    std::string temp = get_temp_x86_reg(Type::PTR_SIZE); // for movzx/mov 64 bit
    emit(get_mov_instr(temp, s2_str, false, size));
    emit(operation + " " + s1_str + ", " + get_sized_register_name(temp, size));
  } else {
    emit(operation + " " + s1_str + ", " + s2_str);
  }
}

void X86_64CodeGenerator::emit(const std::string& instruction) {
  if (!m_call_stack.empty()) {
    m_call_stack.top().arg_instrs.push_back(instruction);
  } else if (m_is_buffering_function) {
    m_current_func_asm_buffer.push_back("\t" + instruction);
  } else {
    m_out << "\t" << instruction << "\n";
  }
}

void X86_64CodeGenerator::emit_label(const std::string& label_name) {
  if (m_is_buffering_function) {
    m_current_func_asm_buffer.push_back(label_name + ":");
  } else {
    m_out << label_name << ":\n";
  }
}

std::string X86_64CodeGenerator::generate(const std::vector<IRInstruction>& instructions, bool is_main_defined) {
  return generate_assembly(instructions, is_main_defined);
}

std::string X86_64CodeGenerator::generate_assembly(const std::vector<IRInstruction>& instructions, bool is_main_defined) {
  clear_func_data();
  m_string_literals_data.clear();
  m_string_literal_to_label.clear();

  /*
  // not necessary anymore to to automatic inclusion of all stdlib
  // in the future perhaps we have an import system for stdlib

  m_out << "extern exit, string_length, print_string, print_char\n";
  m_out << "extern print_newline, print_uint, print_int\n";
  m_out << "extern read_char, read_word, parse_uint\n";
  m_out << "extern parse_int, string_equals, string_copy\n";
  m_out << "extern memcpy, malloc, free, clrscr, string_concat\n";
  m_out << "\n";
  */

  m_out << "default rel\n";
  m_out << "global _start\n";
  m_out << "section .text\n";

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

  handle_begin_func(IRInstruction(IROpCode::BEGIN_FUNC, IR_Label("_start"), {}));

  // emit the top level instructions only, within _start procedure
  m_handling_top_level = true;
  for (const IRInstruction& instr : top_level) {
    handle_instruction(instr);
  }
  m_handling_top_level = false;

  if (is_main_defined) {
    emit("call main");
    emit("mov rdi, rax ; main's return value as exit code");
  } else {
    emit("mov rdi, 0 ; default exit code");
  }
  if (is_main_defined) {
    // argc and argv are provided in the stack frame of _start
    //  but we have to offset due to handle_begin_func of _start pushing rbp
    emit("mov rdi, [rbp+8] ; argc -> 1st arg for main");
    emit("lea rsi, [rbp+16] ; argv -> 2nd arg for main");
    emit("call main");
    emit("mov rdi, rax ; main's return value as exit code");
  } else {
    emit("mov rdi, 0 ; default exit code");
  }


  handle_end_func(true);

  // process all functions underneath the top level instructions
  for (const auto& instr : functions) {
    handle_instruction(instr);
  }

  // output the gathered string labels in the data section
  m_out << "\n";
  m_out << "section .data\n";
  for (const std::string& str : m_string_literals_data) {
    m_out << m_string_literal_to_label.at(str) << ":\n";
    if (str.size() == 1 && str[0] == '\n') {
      m_out << "\tdb 10, 0\n";
      continue;
    }
    m_out << "\tdb \"";
    for (char c : str) {
      if (c == '"')
        m_out << "\", `\"`, \""; // NASM escape for quote
      else if (c == '\n')
        m_out << "\", 10, \""; // Newline
      else if (c == '\t')
        m_out << "\", 9, \""; // Tab
      else if (c == 0)
        m_out << "\", 0, \""; // Explicit null
      else if (c < 32 || c > 126) {
        m_out << "\", " << static_cast<int>(c) << ", \"";
      } else {
        m_out << c;
      }
    }
    // always end with a null byte just in case
    m_out << "\", 0\n";
  }

  if (m_global_var_alloc > 0) {
    m_out << "section .bss\n";
    emit("global_vars resb " + std::to_string(get_align(m_global_var_alloc)));
  }

  /* insert runtime stdlib */
  m_out << "\n" << RUNTIME_ASM_CODE << "\n";

  return m_out.str();
}

void X86_64CodeGenerator::handle_instruction(const IRInstruction& instr) {
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
    case IROpCode::MUL: handle_prim_binop("imul", instr); break;
    case IROpCode::DIV: handle_div(instr); break;
    case IROpCode::MOD: handle_mod(instr); break;
    case IROpCode::NEG: handle_neg(instr); break;
    case IROpCode::CMP_EQ:
    case IROpCode::CMP_NE:
    case IROpCode::CMP_LT:
    case IROpCode::CMP_LE:
    case IROpCode::CMP_GT:
    case IROpCode::CMP_GE: handle_cmp(instr); break;
    case IROpCode::CMP_STR_EQ: handle_cmp_str_eq(instr); break;
    case IROpCode::NOT: handle_logical_not(instr); break;
    case IROpCode::AND: handle_logical_and_or(instr, "and"); break;
    case IROpCode::OR: handle_logical_and_or(instr, "or"); break;
    case IROpCode::BIT_AND: handle_prim_binop("and", instr); break;
    case IROpCode::BIT_OR: handle_prim_binop("or", instr); break;
    case IROpCode::BIT_XOR: handle_prim_binop("xor", instr); break;
    case IROpCode::SHL: handle_shift("shl", instr); break;
    case IROpCode::SHR: handle_shift("shr", instr); break;
    case IROpCode::LABEL: handle_label(instr); break;
    case IROpCode::GOTO: handle_goto(instr); break;
    case IROpCode::IF_Z: handle_if_z(instr); break;
    case IROpCode::BEGIN_LCALL_PREP: m_call_stack.push(CallContext{}); break;
    case IROpCode::PUSH_ARG: handle_push_arg(instr); break;
    case IROpCode::LCALL: handle_lcall(instr); break;
    case IROpCode::ASM_BLOCK: handle_asm_block(instr); break;
    case IROpCode::ADDR_OF: handle_addr_of(instr); break;
    case IROpCode::ALLOC: handle_alloc(instr); break;
    case IROpCode::ALLOC_ARRAY: handle_alloc_array(instr); break;
    case IROpCode::FREE: handle_free(instr); break;
    case IROpCode::MEM_COPY: handle_mem_copy(instr); break;
    default:
      _assert(false, "Unsupported IR opcode for x86_64: " + std::to_string((int)instr.opcode));
  }
}

void X86_64CodeGenerator::clear_func_data() {
  // clear all old local data to the previous function
  m_is_buffering_function = false;
  m_current_func_asm_buffer.clear();
  m_current_func_stack_offset = 0;
  m_ir_reg_to_x86_reg.clear();
  m_x86_reg_to_ir_reg.clear();
  m_var_locations.clear();
  m_reg_count = 0;
  m_spilled_ir_reg_locations.clear();

  m_call_stack = std::stack<CallContext>();
}

void X86_64CodeGenerator::handle_begin_func(const IRInstruction& instr) {
  // instr.result has IR_Label func_label
  clear_func_data();

  const IROperand& label = instr.result.value();
  emit_label(std::get<IR_Label>(label).name);

  // Save base ptr as stack context of this function.
  // This will be used for relative addressing of variables in the program
  emit("push rbp");
  emit("mov rbp, rsp");

  // mark the location for where we have to add the total required stack size
  // for this function. This idx should be overwritten in handle_end_func.
  m_current_func_alloc_placeholder_idx = m_current_func_asm_buffer.size();
  m_is_buffering_function = true; // start buffering

  m_current_func_asm_buffer.push_back(""); // stack offset placeholder
  m_current_func_asm_buffer.push_back(""); // callee saved reg placeholder
}

void X86_64CodeGenerator::handle_end_preamble() {
  // find padding required to ensure 16-byte alignment
  //  AFTER callee-saved registers are pushed
  size_t callee_size = get_used_callee_regs().size() * 8;
  size_t total_stack = m_current_func_stack_offset + callee_size;
  total_stack = get_align(total_stack); // round up to multiple of 16
  size_t required_sub = total_stack - callee_size;

  // perform function post processing
  size_t idx = m_current_func_alloc_placeholder_idx;
  if (idx < m_current_func_asm_buffer.size()) {
    std::string& place_one = m_current_func_asm_buffer[idx];
    if (required_sub > 0) {
      place_one = "\tsub rsp, " + std::to_string(required_sub);
    }

    // save callee-saved registers that are in use
    std::string& placeheld = m_current_func_asm_buffer[idx + 1];
    for (const std::string& used_reg : get_used_callee_regs()) {
      if (!placeheld.empty()) placeheld += "\n";
      placeheld += "\tpush " + used_reg + " ; saving callee-saved register";
    }
  }

  // write the buffered asm contents:
  for (size_t i = 0; i < m_current_func_asm_buffer.size(); ++i) {
    if (i < 2 && m_current_func_asm_buffer[i].empty()) {
      // skip the placeholders if they are empty
      continue;
    }
    m_out << m_current_func_asm_buffer[i] << "\n";
  }
  m_current_func_asm_buffer.clear();
  m_is_buffering_function = false;

  // restore callee-saved registers in reverse order
  const std::vector<std::string>& used_regs = get_used_callee_regs();
  for (int i = used_regs.size() - 1; i >= 0; --i) {
    emit("pop " + used_regs[i] + " ; restoring callee-saved register");
  }
}

void X86_64CodeGenerator::handle_end_func(bool is_exit) {
  // return value is handled by a RETURN operand prior to this.
  // handle the preamble after setting the return value due to the case
  // that the IR reg is a part of the callee-saved regs that are restored
  handle_end_preamble();

  // restore stack pointer and base ptr
  emit("mov rsp, rbp ; restore stack");
  emit("pop rbp");

  if (is_exit) {
    emit("call exit");
  } else {
    // return from procedure here
    emit("ret");
  }
}

// fills 32-bit edi and calls exit
void X86_64CodeGenerator::handle_exit(const IRInstruction& instr) {
  _assert(!instr.operands.empty(), "exit instruction must have an operand, if none is provided in source"
                                   "the IR should provide the default of zero");
  IROperand op = instr.operands[0];
  std::string exit_code = get_sized_component(op, instr.size);
  emit(get_mov_instr("rdi", exit_code, is_imm(op), instr.size) + " ; set exit code");
  emit("call exit");
}

void X86_64CodeGenerator::handle_return(const IRInstruction& instr) {
  if (!instr.operands.empty()) {
    IROperand op = instr.operands[0];
    std::string return_str = get_sized_component(op, instr.size);
    emit(get_mov_instr("rax", return_str, is_imm(op), instr.size) +
         " ; set return value");
  } else {
    emit("xor rax, rax ; default return zero");
  }
  /* goto from IR implementation ensures that
      we jump to the function epilogue from here */
}

void X86_64CodeGenerator::handle_assign(const IRInstruction& instr) {
  // IRInstruction: result: dest_var_or_temp, operands: src_operand
  IROperand dst = instr.result.value();
  IROperand src = instr.operands[0];

  std::uint64_t dst_size = instr.size;
  std::uint64_t src_size = dst_size;
  if (std::holds_alternative<IR_Variable>(src)) {
    src_size = std::get<IR_Variable>(src).size;
  }

  std::string dst_str = get_sized_component(dst, dst_size);
  std::string src_str = get_sized_component(src, src_size);

  bool is_func_decl = std::holds_alternative<IR_Variable>(src) &&
                      std::get<IR_Variable>(src).is_func_decl;
  bool is_string_literal = std::holds_alternative<std::string>(src);

  // case for if the source is a function, where destination is a func ptr
  if (is_func_decl || is_string_literal) {
    std::string temp = get_temp_x86_reg(Type::PTR_SIZE);
    emit("lea " + temp + ", [rel " + src_str + "]");
    src_str = temp;
  }

  if (dst_str.back() == ']' && src_str.back() == ']') {
    // Memory to memory assignment
    std::string temp = get_temp_x86_reg(Type::PTR_SIZE);
    emit(get_mov_instr(temp, src_str, false, src_size));
    emit("mov " + dst_str + ", " + get_sized_register_name(temp, dst_size));
  } else if (std::holds_alternative<IR_Register>(dst)) {
    // Register destination, expression -> reg
    std::string reg_64 = get_x86_reg(std::get<IR_Register>(dst));
    emit(get_mov_instr(reg_64, src_str, is_imm(src), src_size));
  } else {
    // reg/imm -> memory
    if (src_size < dst_size && !is_imm(src)) {
      std::string temp = get_temp_x86_reg(Type::PTR_SIZE);
      emit(get_mov_instr(temp, src_str, false, src_size));
      emit("mov " + dst_str + ", " + get_sized_register_name(temp, dst_size));
    } else {
      emit("mov " + dst_str + ", " + src_str);
    }
  }
}

// handles: 'add', 'sub', 'imul', 'xor', (bitwise)'and'
void X86_64CodeGenerator::handle_prim_binop(const std::string& x86_instr,
                                            const IRInstruction& instr) {
  _assert(std::holds_alternative<IR_Register>(instr.result.value()),
          x86_instr + " instructions must have a register as the destination");
  IROperand dst_op = instr.result.value();
  std::string dst_reg = get_x86_reg(std::get<IR_Register>(dst_op));
  std::string dst_str = get_sized_register_name(dst_reg, instr.size);
  std::string src1_str = get_sized_component(instr.operands[0], instr.size);
  std::string src2_str; // trying to avoid spilling before usage

  emit(get_mov_instr(dst_reg, src1_str, is_imm(instr.operands[0]), instr.size));

  if (x86_instr != "imul" || instr.size == Type::PTR_SIZE) {
    src2_str = get_sized_component(instr.operands[1], instr.size);
    emit(x86_instr + " " + dst_str + ", " + src2_str);
  } else {
    // imul of non-8 byte instruction
    std::string temp = get_temp_x86_reg(Type::PTR_SIZE);
    src2_str = get_sized_component(instr.operands[1], instr.size);

    bool is_src2_imm = is_imm(instr.operands[1]);
    emit(get_mov_instr(temp, src2_str, is_src2_imm, instr.size) +
        " ; 64 bit dst for imul");

    emit(x86_instr + " " + dst_reg + ", " + temp);
  }
}

void X86_64CodeGenerator::handle_div(const IRInstruction& instr) {
  // idiv r/m64: RDX:RAX / r/m64. Quotient in RAX, Remainder in RDX.
  IROperand dst_op = instr.result.value();
  _assert(std::holds_alternative<IR_Register>(dst_op),
          "Div instructions must have a register as the destination");

  emit("; --- DIV start ---");
  emit("push rax");
  emit("push rdx");

  // RDX:RAX / src2. Quotient in RAX, Remainder in RDX.
  std::string temp = get_temp_x86_reg(Type::PTR_SIZE);
  std::string src2_str = get_sized_component(instr.operands[1], instr.size);
  bool s2_imm = is_imm(instr.operands[1]);
  emit(get_mov_instr(temp, src2_str, s2_imm, instr.size) +
       " ; 64 bit dst for idiv");

  // Load dividend (src1) into RAX.
  std::string src1_str = get_sized_component(instr.operands[0], instr.size);
  bool s1_imm = is_imm(instr.operands[0]);
  // sign-extend smaller types into 64-bit RAX to make sure negative numbers work.
  if (s1_imm || instr.size == 8) {
    emit(get_mov_instr("rax", src1_str, s1_imm, instr.size));
  } else if (instr.size == 4) {
    emit("movsxd rax, " + src1_str + " ; sign-extend 32-bit dividend");
  } else {
    emit("movsx rax, " + get_size_prefix(instr.size) + " " + src1_str + " ; sign-extend small dividend");
  }

  emit("cqo");
  emit("idiv " + temp);

  std::string dst_str = get_sized_component(dst_op, Type::PTR_SIZE);
  emit("mov " + dst_str + ", rax"); // store 64 bit result
  emit("pop rdx");
  emit("pop rax");
  emit("; --- DIV end ---");
}

void X86_64CodeGenerator::handle_mod(const IRInstruction& instr) {
  // idiv r/m64: RDX:RAX / r/m64. Quotient in RAX, Remainder in RDX.
  IROperand dst_op = instr.result.value();
  _assert(std::holds_alternative<IR_Register>(dst_op),
          "Mod instructions must have a register as the destination");

  emit("; --- MOD start ---");
  emit("push rax");
  emit("push rdx");

  std::string temp = get_temp_x86_reg(Type::PTR_SIZE);
  std::string src2_str = get_sized_component(instr.operands[1], instr.size);
  bool s2_imm = is_imm(instr.operands[1]);
  emit(get_mov_instr(temp, src2_str, s2_imm, instr.size) +
       " ; 64 bit dst for idiv (for mod)");

  std::string src1_str = get_sized_component(instr.operands[0], instr.size);
  bool s1_imm = is_imm(instr.operands[0]);
  if (s1_imm || instr.size == 8) {
    emit(get_mov_instr("rax", src1_str, s1_imm, instr.size));
  } else if (instr.size == 4) {
    emit("movsxd rax, " + src1_str + " ; sign-extend 32-bit dividend");
  } else {
    emit("movsx rax, " + get_size_prefix(instr.size) + " " + src1_str + " ; sign-extend small dividend");
  }

  emit("cqo");
  emit("idiv " + temp);

  std::string dst_str = get_sized_component(dst_op, Type::PTR_SIZE);
  emit("mov " + dst_str + ", rdx"); // store 64 bit remainder
  emit("pop rdx");
  emit("pop rax");
  emit("; --- MOD end ---");
}

void X86_64CodeGenerator::handle_neg(const IRInstruction& instr) {
  _assert(std::holds_alternative<IR_Register>(instr.result.value()),
          "Neg instructions must have a register as the destination");
  IROperand dst_op = instr.result.value();
  std::string dst_reg = get_x86_reg(std::get<IR_Register>(dst_op));
  std::string dst_str = get_sized_register_name(dst_reg, instr.size);
  std::string src_str = get_sized_component(instr.operands[0], instr.size);

  emit(get_mov_instr(dst_reg, src_str, is_imm(instr.operands[0]), instr.size));
  emit("neg " + dst_str);
}

void X86_64CodeGenerator::handle_logical_not(const IRInstruction& instr) {
  _assert(std::holds_alternative<IR_Register>(instr.result.value()),
          "Not instructions must have a register as the destination");
  IROperand dst_op = instr.result.value();
  std::string dst_reg = get_x86_reg(std::get<IR_Register>(dst_op));
  std::string dst_str = get_sized_register_name(dst_reg, instr.size);
  std::string src_str = get_sized_component(instr.operands[0], instr.size);

  emit(get_mov_instr(dst_reg, src_str, is_imm(instr.operands[0]), instr.size));

  // evaluate if equal to zero
  emit("cmp " + dst_str + ", 0");
  emit("sete al");
  emit("movzx " + dst_str + ", al");
}

void X86_64CodeGenerator::handle_logical_and_or(
    const IRInstruction& instr, const std::string& op_mnemonic) {
  _assert(std::holds_alternative<IR_Register>(instr.result.value()),
          "Or instructions must have a register as the destination");
  IROperand dst_op = instr.result.value();
  std::string dst_reg = get_x86_reg(std::get<IR_Register>(dst_op));
  std::string dst_str = get_sized_register_name(dst_reg, instr.size);
  std::string src1_str = get_sized_component(instr.operands[0], instr.size);
  std::string src2_str = get_sized_component(instr.operands[1], instr.size);

  emit(get_mov_instr(dst_reg, src1_str, is_imm(instr.operands[0]), instr.size));
  emit(op_mnemonic + " " + dst_str + ", " + src2_str);
}

void X86_64CodeGenerator::handle_shift(const std::string& x86_instr, const IRInstruction& instr) {
  _assert(std::holds_alternative<IR_Register>(instr.result.value()),
      x86_instr + " instructions must have a register as the destination");

  IROperand dst_op = instr.result.value();
  std::string dst_reg = get_x86_reg(std::get<IR_Register>(dst_op));
  std::string dst_str = get_sized_register_name(dst_reg, instr.size);

  std::string src1_str = get_sized_component(instr.operands[0], instr.size);
  emit(get_mov_instr(dst_reg, src1_str, is_imm(instr.operands[0]), instr.size));
  if (is_imm(instr.operands[1])) {
    std::string shift_amt = std::to_string(std::get<IR_Immediate>(instr.operands[1]).val);
    emit(x86_instr + " " + dst_str + ", " + shift_amt);
  } else {
    bool rcx_pushed = false; /* shift must be in CL register; push to avoid clobbering */
    if (dst_reg != "rcx") { // Don't push if our destination IS rcx
      emit("push rcx");
      rcx_pushed = true;
    }
    std::string shift_amt_str = get_sized_component(instr.operands[1], instr.size);
    emit(get_mov_instr("rcx", shift_amt_str, false, instr.size) + " ; load shift amount into rcx");
    emit(x86_instr + " " + dst_str + ", cl");
    if (rcx_pushed) {
      emit("pop rcx");
    }
  }
}

void X86_64CodeGenerator::handle_cmp(const IRInstruction& instr) {
  // Result of CMP is boolean, stored in dest_reg
  // Operands are src1, src2
  IROperand dst = instr.result.value();
  _assert(std::holds_alternative<IR_Register>(dst),
          "Cmp instructions must have a register as the destination");

  std::string dst_str = get_sized_component(dst, Type::PTR_SIZE);

  // If the destination is rax, then we don't have to save its context,
  // otherwise, we have to push it, as we will be using the lower byte `al`
  bool rax_pushed = false;
  if (dst_str != "rax") {
    emit("push rax");
    rax_pushed = true;
  }

  // operands cannot both be from memory
  emit_one_operand_memory_operation(instr.operands[0], instr.operands[1], "cmp",
                                    instr.size);
  std::string set_instr;
  std::string byte_reg = "al";
  switch (instr.opcode) {
    case IROpCode::CMP_EQ: set_instr = "sete"; break;
    case IROpCode::CMP_NE: set_instr = "setne"; break;
    case IROpCode::CMP_LT: set_instr = "setl"; break;
    case IROpCode::CMP_LE: set_instr = "setle"; break;
    case IROpCode::CMP_GT: set_instr = "setg"; break;
    case IROpCode::CMP_GE: set_instr = "setge"; break;
    default: _assert(false, "Unhandled CMP type in handle_cmp");
  }
  emit(set_instr + " " + byte_reg);
  emit("movzx " + dst_str + ", " + byte_reg); // zero-extend al to dest_reg
  if (rax_pushed) emit("pop rax");
}

void X86_64CodeGenerator::handle_cmp_str_eq(const IRInstruction& instr) {
  IROperand dst = instr.result.value();
  _assert(std::holds_alternative<IR_Register>(dst),
          "CMP_STR_EQ instruction must have a register as the destination");
  _assert(instr.operands.size() == 2, "str_eq should have two operands");
  _assert(instr.size == Type::PTR_SIZE, "str_eq should have 8 byte instr size");

  std::string s1_str = get_sized_component(instr.operands[0], instr.size);
  std::string s2_str = get_sized_component(instr.operands[1], instr.size);

  m_call_stack.push(CallContext{});
  save_caller_saved_regs();

  CallContext ctx = m_call_stack.top();
  m_call_stack.pop();

  for (const std::string& arg_instr : ctx.arg_instrs) {
    emit(arg_instr);
  }

  std::string t1 = get_temp_x86_reg(Type::PTR_SIZE);
  std::string t2 = get_temp_x86_reg(Type::PTR_SIZE);
  emit(get_mov_instr(t1, s1_str, is_imm(instr.operands[0]), instr.size));
  emit(get_mov_instr(t2, s2_str, is_imm(instr.operands[1]), instr.size));

  emit("mov rdi, " + t1 + " ; arg1 for string_equals");
  emit("mov rsi, " + t2 + " ; arg2 for string_equals");

  emit("call string_equals");
  restore_caller_saved_regs(ctx.used_caller_saved);

  std::string dst_str = get_sized_component(dst, Type::PTR_SIZE);
  emit("movzx " + dst_str + ", al"); // zero-extend al to dest_reg
}

void X86_64CodeGenerator::handle_label(const IRInstruction& instr) {
  const IROperand& label = instr.result.value();
  _assert(std::holds_alternative<IR_Label>(label),
          "label instruction should be a label operand");
  emit_label(std::get<IR_Label>(label).name);
}

void X86_64CodeGenerator::handle_goto(const IRInstruction& instr) {
  const IROperand& label = instr.operands[0];
  _assert(std::holds_alternative<IR_Label>(label),
          "goto label should be a label operand");
  emit("jmp " + std::get<IR_Label>(label).name);
}

void X86_64CodeGenerator::handle_if_z(const IRInstruction& instr) {
  // instr.operands[0] is the condition operand
  // instr.operands[1] is the target label
  const IROperand& label = instr.operands[1];
  _assert(std::holds_alternative<IR_Label>(label),
          "if_z label should be a label operand");

  if (is_imm(instr.operands[0])) {
    // if the condition is literal 0, we will always jump
    if (std::get<IR_Immediate>(instr.operands[0]).val == 0) {
      emit("jmp " + std::get<IR_Label>(label).name);
    }
    // if it is non-zero, it will never jump. We do nothing and fall-through.
  } else {
    // For variables and registers, compare directly against 0
    std::string s1_str = get_sized_component(instr.operands[0], instr.size);
    emit("cmp " + s1_str + ", 0");
    emit("jz " + std::get<IR_Label>(label).name);
  }
}

void X86_64CodeGenerator::handle_push_arg(const IRInstruction& instr) {
  IROperand src = instr.operands[0];

  std::uint64_t arg_size = instr.size;
  CallContext& ctx = m_call_stack.top();

  if (ctx.current_args_passed < m_arg_regs.size()) {
    // handle register arguments (first 6 args)
    const std::string& arg_64 = m_arg_regs[ctx.current_args_passed++];
    std::string src_str = get_sized_component(src, arg_size);
    std::string mov = get_mov_instr(arg_64, src_str, is_imm(src), arg_size);
    ctx.arg_instrs.push_back(mov + "; value stored in arg reg");
    return;
  }

  // handle stack arguments (args beyond the first 6), note it requires QWORD sz
  std::string src_str = get_sized_component(src, arg_size);

  // right now it should be save to use RAX, no potential clobbering
  std::string mov = get_mov_instr("rax", src_str, is_imm(src), instr.size);

  size_t boundary_idx = ctx.current_args_passed;
  auto it = ctx.arg_instrs.begin() + boundary_idx;
  it = ctx.arg_instrs.insert(it, mov);
  ctx.arg_instrs.insert(it + 1, "push rax ; pushing arg to stack");
  ctx.stack_args_size += Type::PTR_SIZE; // we must push 8 bytes
}

void X86_64CodeGenerator::save_caller_saved_regs() {
  if (m_call_stack.empty()) return;
  CallContext& ctx = m_call_stack.top();

  std::vector<std::string> regs_to_save;
  for (const std::string& reg : m_caller_saved_regs) {
    auto it = m_x86_reg_to_ir_reg.find(reg);
    if (it != m_x86_reg_to_ir_reg.end() && it->second.first != -1) {
      regs_to_save.push_back(reg);
    }
  }

  std::vector<std::string> pushes;
  for (const std::string& reg : regs_to_save) {
    pushes.push_back("push " + reg + " ; saving caller-saved register " + reg);
    ctx.used_caller_saved.push_back(reg);
  }
  // insert at beginning so caller-saved registers don't offset stack args
  ctx.arg_instrs.insert(ctx.arg_instrs.begin(), pushes.begin(), pushes.end());

}

void X86_64CodeGenerator::restore_caller_saved_regs(const std::vector<std::string>& used_caller_saved) {
  for (int i = used_caller_saved.size() - 1; i >= 0; --i) {
    const std::string& arg = used_caller_saved[i];
    emit("pop " + arg + " ; restoring caller-saved register");
  }
}

std::vector<std::string> X86_64CodeGenerator::get_used_callee_regs() {
  std::vector<std::string> used;
  for (const std::string& reg : m_callee_saved_regs) {
    auto it = m_x86_reg_to_ir_reg.find(reg);
    if (it != m_x86_reg_to_ir_reg.end() && it->second.first != -1) {
      used.push_back(reg);
    }
  }
  return used;
}

void X86_64CodeGenerator::handle_lcall(const IRInstruction& instr) {
  const IROperand& label = instr.operands[0];
  _assert(std::holds_alternative<IR_Label>(label) ||
              std::holds_alternative<IR_Variable>(label),
          "lcall should be from a label or variable (func ptr) operand");

  save_caller_saved_regs();
  CallContext ctx = m_call_stack.top();
  m_call_stack.pop();

  for (const std::string& arg_instr : ctx.arg_instrs) {
    emit(arg_instr);
  }

  // special handling for read_word
  const std::string& name = std::holds_alternative<IR_Label>(label)
                                ? std::get<IR_Label>(label).name
                                : get_sized_component(label, Type::PTR_SIZE);
  if (name == "read_word") {
    // just doing 64 byte input buffer for now for the read procedure
    emit("sub rsp, 64");
    emit("mov rdi, rsp");
  }

  emit("call " + name);

  if (name == "read_word") {
    // deallocate the memory
    emit("add rsp, 64 ; clean up read_word buffer");
  }

  // restore stack pointer by adding back the total stack space used
  if (ctx.stack_args_size > 0) {
    emit("add rsp, " + std::to_string(ctx.stack_args_size) +
         " ; restore stack after call for args");
  }

  // restore caller-saved registers
  restore_caller_saved_regs(ctx.used_caller_saved);

  // if the call has a result, it's in rax: move it to the IR result register.
  if (instr.result.has_value()) {
    IROperand res = instr.result.value();
    _assert(std::holds_alternative<IR_Register>(res),
            "LCALL result must be a register");
    std::string dest_reg = get_sized_component(res, Type::PTR_SIZE);
    std::string sized_rax = get_sized_register_name("rax", instr.size);
    emit(get_mov_instr(dest_reg, sized_rax, false, instr.size));
  }
}

void X86_64CodeGenerator::handle_asm_block(const IRInstruction& instr) {
  _assert(!instr.operands.empty() &&
              std::holds_alternative<std::string>(instr.operands[0]),
          "std::string operand must exist for ASM_BLOCK in IR");

  const std::string& asm_code = std::get<std::string>(instr.operands[0]);
  emit(asm_code);
}

void X86_64CodeGenerator::handle_load(const IRInstruction& instr) {
  // IRInstruction: result: dst, operands[0]: address_operand(*addr)
  // size is the size of the register dst
  // dst = [mem]
  IROperand dst = instr.result.value();
  IROperand src_addr = instr.operands[0];

  std::string dst_str = get_sized_component(dst, instr.size);
  std::string src_addr_str = get_sized_component(src_addr, Type::PTR_SIZE);

  // if the pointer address is stored in memory, move to a temp
  if (src_addr_str.back() == ']') {
    std::string temp = get_temp_x86_reg(Type::PTR_SIZE);
    emit("mov " + temp + ", " + src_addr_str);
    src_addr_str = temp;
  }

  std::string deref_src = get_size_prefix(instr.size) + " [" + src_addr_str + "]";

  // load the value from the dereferenced address
  if (dst_str.back() == ']') {
    // dst is memory, route through a temp reg
    std::string temp_val = get_temp_x86_reg(Type::PTR_SIZE);
    emit(get_mov_instr(temp_val, deref_src, false, instr.size) + " ; load to temp");
    emit("mov " + dst_str + ", " + get_sized_register_name(temp_val, instr.size) +
         " ; store to memory dest");
  } else {
    // direct load to register
    std::string dst_reg_64 = operand_to_string(dst); // full 64-bit name so that mov_instr will zero-extend
    emit(get_mov_instr(dst_reg_64, deref_src, false, instr.size) + " ; performing load");
  }
}

void X86_64CodeGenerator::handle_store(const IRInstruction& instr) {
  // IRInstruction: result: address_dest(*addr), operands[0]: src
  // [mem] = src
  IROperand dst_addr = instr.result.value();
  IROperand src_val = instr.operands[0];

  std::string dst_addr_str = get_sized_component(dst_addr, Type::PTR_SIZE);
  std::string src_val_str = get_sized_component(src_val, instr.size);

  // if dst pointer is in memory, move it to a register
  if (dst_addr_str.back() == ']') {
    std::string temp = get_temp_x86_reg(Type::PTR_SIZE);
    emit("mov " + temp + ", " + dst_addr_str);
    dst_addr_str = temp;
  }

  // if src value is in mem, also move it to a reg
  if (src_val_str.back() == ']') {
    std::string temp_val = get_temp_x86_reg(Type::PTR_SIZE);
    emit(get_mov_instr(temp_val, src_val_str, false, instr.size));
    src_val_str = get_sized_register_name(temp_val, instr.size);
  }

  emit("mov " + get_size_prefix(instr.size) + " [" + dst_addr_str + "], " +
       src_val_str + " ; performing store");
}

void X86_64CodeGenerator::handle_addr_of(const IRInstruction& instr) {
  _assert(instr.result.has_value() &&
              std::holds_alternative<IR_Register>(instr.result.value()),
          "addrof must have a register result");
  _assert(instr.operands.size() == 1, "addrof should have one operand");
  _assert(instr.size == Type::PTR_SIZE, "addrof should have an instr size: 8");

  const IROperand& dst = instr.result.value();
  std::string dst_str = get_sized_component(dst, instr.size);

  IROperand src = instr.operands[0];
  std::string src_str = operand_to_string(src);

  if (src_str.back() == ']') {
    emit("lea " + dst_str + ", " + src_str);
  } else {
    emit("mov " + dst_str + ", " + src_str);
  }
}

void X86_64CodeGenerator::handle_alloc(const IRInstruction& instr) {
  _assert(instr.result.has_value() &&
              std::holds_alternative<IR_Register>(instr.result.value()),
          "alloc must have a register result");
  _assert(!instr.operands.empty() &&
              std::holds_alternative<IR_Immediate>(instr.operands[0]),
          "alloc should have one imm operand at [0]");

  const IROperand& dst = instr.result.value();
  std::uint64_t type_alloc_size = std::get<IR_Immediate>(instr.operands[0]).val;

  m_call_stack.push(CallContext{});
  save_caller_saved_regs();
  CallContext ctx = m_call_stack.top();
  m_call_stack.pop();

  for (const std::string& arg_instr : ctx.arg_instrs) emit(arg_instr);

  emit("mov rdi, " + std::to_string(type_alloc_size));
  emit("call malloc");

  restore_caller_saved_regs(ctx.used_caller_saved);

  std::string dst_ptr_str = get_sized_component(dst, Type::PTR_SIZE);
  emit("mov " + dst_ptr_str + ", rax"); // no need for movzx

  if (instr.operands.size() > 1) {
    const IROperand& initializer_op = instr.operands[1];
    std::uint64_t init_type_size = instr.size;
    _assert(init_type_size > 0, "Initializer present but its type size is 0");

    std::string init_val = get_sized_component(initializer_op, init_type_size);
    std::string allocated = get_size_prefix(init_type_size) + " [rax]";

    emit_one_operand_memory_operation(allocated, init_val, "mov", init_type_size);
  }
}

void X86_64CodeGenerator::handle_alloc_array(const IRInstruction& instr) {
  _assert(instr.result.has_value() &&
              std::holds_alternative<IR_Register>(instr.result.value()),
          "alloc array must have a register result");
  _assert(instr.operands.size() == 2 &&
              std::holds_alternative<IR_Immediate>(instr.operands[0]),
          "alloc array should have two operands, the first an immediate");

  const IROperand& dst = instr.result.value();

  std::uint64_t element_size_val = std::get<IR_Immediate>(instr.operands[0]).val;
  IROperand num_elements_op = instr.operands[1];
  std::uint64_t num_elements_type_size = instr.size;
  _assert(num_elements_type_size > 0,
          "num_elements type operand must have a valid type size");

  std::string num_elements_str =
      get_sized_component(num_elements_op, num_elements_type_size);

  std::string temp = get_temp_x86_reg(Type::PTR_SIZE);
  emit(get_mov_instr(temp, num_elements_str, is_imm(num_elements_op),
                     num_elements_type_size));
  emit("imul " + temp + ", " + std::to_string(element_size_val));

  m_call_stack.push(CallContext{});
  save_caller_saved_regs();
  CallContext ctx = m_call_stack.top();
  m_call_stack.pop();

  for (const std::string& arg_instr : ctx.arg_instrs) emit(arg_instr);

  emit("mov rdi, " + temp);
  emit("call malloc");

  restore_caller_saved_regs(ctx.used_caller_saved);

  std::string dst_ptr_str = get_sized_component(dst, Type::PTR_SIZE);
  emit("mov " + dst_ptr_str + ", rax"); // no need for movzx
}

void X86_64CodeGenerator::handle_free(const IRInstruction& instr) {
  _assert(instr.operands.size() == 1, "free instr should have one operand");
  IROperand ptr_op = instr.operands[0];
  std::string ptr_str = get_sized_component(ptr_op, Type::PTR_SIZE);

  m_call_stack.push(CallContext{});
  save_caller_saved_regs();
  CallContext ctx = m_call_stack.top();
  m_call_stack.pop();

  for (const std::string& arg_instr : ctx.arg_instrs) emit(arg_instr);

  emit("mov rdi, " + ptr_str); // no need for movzx
  emit("call free");

  restore_caller_saved_regs(ctx.used_caller_saved);

  emit("mov " + ptr_str + ", 0 ; clear ptr to null after free");
}

void X86_64CodeGenerator::handle_mem_copy(const IRInstruction& instr) {
  // result: dst_addr, operands: [src_addr ], size: size to copy
  IROperand dst_op = instr.result.value();
  IROperand src_op = instr.operands[0];

  std::string dst_str = get_sized_component(dst_op, Type::PTR_SIZE);
  std::string src_str = get_sized_component(src_op, Type::PTR_SIZE);

  m_call_stack.push(CallContext{});
  save_caller_saved_regs();
  CallContext ctx = m_call_stack.top();
  m_call_stack.pop();

  for (const std::string& arg_instr : ctx.arg_instrs) emit(arg_instr);

  emit("mov rdi, " + dst_str + " ; memcpy dst");
  emit("mov rsi, " + src_str + " ; memcpy src");
  emit("mov rcx, " + std::to_string(instr.size) + " ; memcpy size");
  emit("call memcpy");

  restore_caller_saved_regs(ctx.used_caller_saved);
}
