#include "ir_generator.h"
#include "../../util.h"

IrGenerator::IrGenerator() : m_next_reg_id(0), m_next_label_id(0) {}

IR_Register IrGenerator::new_temp_reg() { return IR_Register(m_next_reg_id++); }
IR_Label IrGenerator::new_label() { return IR_Label(m_next_label_id++); }
IR_Label IrGenerator::new_func_label(std::string_view func_name) {
  return IR_Label(std::string(func_name));
}

void IrGenerator::emit_begin_func(IR_Label func_label) {
  m_instructions.emplace_back(IROpCode::BEGIN_FUNC, func_label,
                              std::vector<IROperand>{});
}

void IrGenerator::emit_end_func() {
  m_instructions.emplace_back(IROpCode::END_FUNC);
}

void IrGenerator::emit_return(IROperand return_val, std::uint64_t return_size) {
  m_instructions.emplace_back(IROpCode::RETURN, std::nullopt,
                              std::vector<IROperand>{return_val}, return_size);
}

void IrGenerator::emit_return() {
  m_instructions.emplace_back(IROpCode::RETURN);
}

void IrGenerator::emit_exit(IROperand exit_operand, std::uint64_t sz /* should be u8 */) {
  m_instructions.emplace_back(
      IROpCode::EXIT, std::nullopt,
      std::vector<IROperand>{exit_operand}, sz);
}

void IrGenerator::emit_assign(IROperand dst, IROperand src, std::uint64_t size) {
  _assert_nolog(std::holds_alternative<IR_Register>(dst) ||
                std::holds_alternative<IR_Variable>(dst),
                "dest for assign instr must be a register or variable");
  m_instructions.emplace_back(IROpCode::ASSIGN, dst,
                              std::vector<IROperand>{src}, size);
}

void IrGenerator::emit_load(IR_Register dst, IROperand addr_src,
                            std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::LOAD, dst,
                              std::vector<IROperand>{addr_src}, size);
}

void IrGenerator::emit_store(IROperand address_dest, IROperand src,
                             std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::STORE, address_dest,
                              std::vector<IROperand>{src}, size);
}

void IrGenerator::emit_add(IR_Register dst, IROperand s1, IROperand s2,
                           std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::ADD, dst,
                              std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_sub(IR_Register dst, IROperand s1, IROperand s2,
                           std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::SUB, dst,
                              std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_mul(IR_Register dst, IROperand s1, IROperand s2,
                           std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::MUL, dst,
                              std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_div(IR_Register dst, IROperand s1, IROperand s2,
                           std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::DIV, dst,
                              std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_mod(IR_Register dst, IROperand s1, IROperand s2,
                           std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::MOD, dst,
                              std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_neg(IR_Register dst, IROperand src, std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::NEG, dst, std::vector<IROperand>{src},
                              size);
}

void IrGenerator::emit_log_not(IR_Register dst, IROperand src, std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::NOT, dst, std::vector<IROperand>{src},
                              size);
}

void IrGenerator::emit_log_and(IR_Register dst, IROperand s1, IROperand s2,
                               std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::AND, dst,
                              std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_log_or(IR_Register dst, IROperand s1, IROperand s2,
                              std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::OR, dst, std::vector<IROperand>{s1, s2},
                              size);
}

void IrGenerator::emit_bit_and(IR_Register dst, IROperand s1, IROperand s2, std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::BIT_AND, dst, std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_bit_or(IR_Register dst, IROperand s1, IROperand s2, std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::BIT_OR, dst, std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_bit_xor(IR_Register dst, IROperand s1, IROperand s2, std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::BIT_XOR, dst, std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_shl(IR_Register dst, IROperand s1, IROperand s2, std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::SHL, dst, std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_shr(IR_Register dst, IROperand s1, IROperand s2, std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::SHR, dst, std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_cmp_eq(IR_Register dst, IROperand s1, IROperand s2,
                              std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::CMP_EQ, dst,
                              std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_cmp_ne(IR_Register dst, IROperand s1, IROperand s2,
                              std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::CMP_NE, dst,
                              std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_cmp_lt(IR_Register dst, IROperand s1, IROperand s2,
                              std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::CMP_LT, dst,
                              std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_cmp_le(IR_Register dst, IROperand s1, IROperand s2,
                              std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::CMP_LE, dst,
                              std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_cmp_gt(IR_Register dst, IROperand s1, IROperand s2,
                              std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::CMP_GT, dst,
                              std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_cmp_ge(IR_Register dst, IROperand s1, IROperand s2,
                              std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::CMP_GE, dst,
                              std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_cmp_str_eq(IR_Register dst, IROperand s1, IROperand s2,
                                  std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::CMP_STR_EQ, dst,
                              std::vector<IROperand>{s1, s2}, size);
}

void IrGenerator::emit_label(IR_Label label) {
  m_instructions.emplace_back(IROpCode::LABEL, label, std::vector<IROperand>{});
}

void IrGenerator::emit_goto(IR_Label target) {
  m_instructions.emplace_back(IROpCode::GOTO, std::nullopt,
                              std::vector<IROperand>{target});
}

void IrGenerator::emit_if_z(IROperand cond, IR_Label target,
                            std::uint64_t cond_operands_size) {
  m_instructions.emplace_back(IROpCode::IF_Z, std::nullopt,
                              std::vector<IROperand>{cond, target},
                              cond_operands_size);
}

void IrGenerator::emit_begin_lcall_prep() {
  m_instructions.emplace_back(IROpCode::BEGIN_LCALL_PREP);
}

void IrGenerator::emit_push_arg(IROperand src, std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::PUSH_ARG, std::nullopt,
                              std::vector<IROperand>{src}, size);
}

void IrGenerator::emit_lcall(std::optional<IR_Register> dst,
                             IROperand func_target, std::uint64_t return_size) {
  // func_target is either a function label or function ptr variable
  m_instructions.emplace_back(IROpCode::LCALL, dst,
                              std::vector<IROperand>{func_target}, return_size);
}

void IrGenerator::emit_asm_block(const std::string& asm_code) {
  m_instructions.emplace_back(IROpCode::ASM_BLOCK, std::nullopt,
                              std::vector<IROperand>{asm_code});
}

void IrGenerator::emit_addr_of(IR_Register dst, IROperand src_lval) {
  m_instructions.emplace_back(IROpCode::ADDR_OF, dst,
                              std::vector<IROperand>{src_lval}, Type::PTR_SIZE);
}

void IrGenerator::emit_alloc(IR_Register dst_ptr, std::uint64_t type_size,
                             std::optional<IROperand> initializer,
                             std::uint64_t initializer_type_size) {
  std::vector<IROperand> ops;
  // immediate will occupy ptr_size bytes
  ops.push_back(IR_Immediate(type_size, Type::PTR_SIZE));
  if (initializer.has_value()) {
    ops.push_back(initializer.value());
  }
  m_instructions.emplace_back(
      IROpCode::ALLOC, dst_ptr, std::move(ops),
      initializer.has_value() ? initializer_type_size : 0);
}

void IrGenerator::emit_alloc_array(IR_Register dst_ptr, std::uint64_t size_el,
                                   IROperand num_el,
                                   std::uint64_t initializer_type_size) {
  m_instructions.emplace_back(
      IROpCode::ALLOC_ARRAY, dst_ptr,
      std::vector<IROperand>{IR_Immediate(size_el, Type::PTR_SIZE), num_el},
      initializer_type_size);
}

void IrGenerator::emit_free(IROperand ptr) {
  m_instructions.emplace_back(IROpCode::FREE, std::nullopt,
                              std::vector<IROperand>{ptr}, Type::PTR_SIZE);
}

void IrGenerator::emit_mem_copy(IROperand dst, IROperand src, std::uint64_t size) {
  m_instructions.emplace_back(IROpCode::MEM_COPY, dst,
                              std::vector<IROperand>{src}, size);
}

const std::vector<IRInstruction>& IrGenerator::get_instructions() const {
  return m_instructions;
}
