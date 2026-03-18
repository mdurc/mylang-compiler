#include "ir_printer.h"

#include <variant>

#include "../../util.h"

void print_ir_register(const IR_Register& reg, std::ostream& out) {
  out << "_t" << reg.id;
}

void print_ir_variable(const IR_Variable& var, std::ostream& out) {
  out << var.name;
}

void print_ir_parameter_slot(const IR_ParameterSlot& slot, std::ostream& out) {
  out << "_param " << slot.index;
}

void print_ir_immediate(const IR_Immediate& imm, std::ostream& out) {
  out << imm.val;
}

void print_ir_label(const IR_Label& label, std::ostream& out) {
  out << label.name;
}

void print_ir_operand(const IROperand& operand, std::ostream& out) {
  if (std::holds_alternative<IR_Register>(operand)) {
    print_ir_register(std::get<IR_Register>(operand), out);
  } else if (std::holds_alternative<IR_Variable>(operand)) {
    print_ir_variable(std::get<IR_Variable>(operand), out);
  } else if (std::holds_alternative<IR_Immediate>(operand)) {
    print_ir_immediate(std::get<IR_Immediate>(operand), out);
  } else if (std::holds_alternative<IR_Label>(operand)) {
    print_ir_label(std::get<IR_Label>(operand), out);
  } else if (std::holds_alternative<std::string>(operand)) {
    out << "\"" << escape_string(std::get<std::string>(operand)) << "\"";
  } else if (std::holds_alternative<IR_ParameterSlot>(operand)) {
    print_ir_parameter_slot(std::get<IR_ParameterSlot>(operand), out);
  }
}

void print_ir_instruction(const IRInstruction& instr, std::ostream& out) {
  // Labels are not indented
  if (instr.opcode == IROpCode::LABEL) {
    _assert_nolog(instr.result.has_value(),
                  "label instruction must have a result value");
    print_ir_operand(instr.result.value(), out);
    out << ":\n";
    return;
  }

  if (instr.opcode == IROpCode::BEGIN_FUNC) {
    _assert_nolog(instr.result.has_value(), "begin func must have a result value");
    print_ir_operand(instr.result.value(), out);
    out << "\n";
  }

  out << "\t";

  switch (instr.opcode) {
    case IROpCode::BEGIN_FUNC: out << "BeginFunc"; break;
    case IROpCode::RETURN:
      out << "Return";
      if (!instr.operands.empty()) {
        out << " ";
        print_ir_operand(instr.operands[0], out);
      }
      break;
    case IROpCode::END_FUNC: out << "EndFunc"; break;
    case IROpCode::EXIT: out << "Exit"; break;
    case IROpCode::ASSIGN:
      _assert_nolog(instr.result.has_value() && instr.operands.size() == 1,
              "assign operation must have a value and 1 operand");
      print_ir_operand(instr.result.value(), out);
      out << " = ";
      print_ir_operand(instr.operands[0], out);
      break;
    case IROpCode::LOAD: // dest = *addr
      _assert_nolog(instr.result.has_value() && instr.operands.size() == 1,
              "load operation must have a value and 1 operand");
      print_ir_operand(instr.result.value(), out);
      out << " = *(";
      print_ir_operand(instr.operands[0], out);
      out << ")";
      break;
    case IROpCode::STORE: // *addr = val
      _assert_nolog(instr.result.has_value() && instr.operands.size() == 1,
              "store operation must have a value and 1 operand");
      out << "*(";
      print_ir_operand(instr.result.value(), out);
      out << ") = ";
      print_ir_operand(instr.operands[0], out);
      break;
    case IROpCode::ADD:
    case IROpCode::SUB:
    case IROpCode::MUL:
    case IROpCode::DIV:
    case IROpCode::MOD:
    case IROpCode::AND:
    case IROpCode::OR:
    case IROpCode::CMP_EQ:
    case IROpCode::CMP_NE:
    case IROpCode::CMP_LT:
    case IROpCode::CMP_LE:
    case IROpCode::CMP_GT:
    case IROpCode::CMP_GE:
    case IROpCode::CMP_STR_EQ:
      _assert_nolog(instr.result.has_value() && instr.operands.size() == 2,
              "cmp operation must have a value and 2 operands");
      print_ir_operand(instr.result.value(), out);
      out << " = ";
      print_ir_operand(instr.operands[0], out);
      switch (instr.opcode) {
        case IROpCode::ADD: out << " + "; break;
        case IROpCode::SUB: out << " - "; break;
        case IROpCode::MUL: out << " * "; break;
        case IROpCode::DIV: out << " / "; break;
        case IROpCode::MOD: out << " % "; break;
        case IROpCode::AND: out << " && "; break;
        case IROpCode::OR: out << " || "; break;
        case IROpCode::CMP_EQ: out << " == "; break;
        case IROpCode::CMP_NE: out << " != "; break;
        case IROpCode::CMP_LT: out << " < "; break;
        case IROpCode::CMP_LE: out << " <= "; break;
        case IROpCode::CMP_GT: out << " > "; break;
        case IROpCode::CMP_GE: out << " >= "; break;
        case IROpCode::CMP_STR_EQ: out << " ==[str] "; break;
        default: out << " ?? "; break;
      }
      print_ir_operand(instr.operands[1], out);
      break;
    case IROpCode::NEG:
    case IROpCode::NOT:
      _assert_nolog(instr.result.has_value() && instr.operands.size() == 1,
              "NOT must have a result value and 1 operand");
      print_ir_operand(instr.result.value(), out);
      out << " = ";
      if (instr.opcode == IROpCode::NEG) out << "-";
      if (instr.opcode == IROpCode::NOT) out << "!";
      print_ir_operand(instr.operands[0], out);
      break;
    case IROpCode::GOTO:
      _assert_nolog(!instr.result.has_value() && instr.operands.size() == 1,
              "GOTO must have just 1 operand");
      out << "Goto ";
      print_ir_operand(instr.operands[0], out);
      break;
    case IROpCode::IF_Z: // IfZ cond Goto Lbl
      _assert_nolog(!instr.result.has_value() && instr.operands.size() == 2,
              "IF_Z must have just 2 operands");
      out << "IfZ ";
      print_ir_operand(instr.operands[0], out);
      out << " Goto ";
      print_ir_operand(instr.operands[1], out);
      break;
    case IROpCode::BEGIN_LCALL_PREP: out << "BeginLCallPrep"; break;
    case IROpCode::PUSH_ARG:
      _assert_nolog(!instr.result.has_value() && instr.operands.size() == 1,
              "PushArg must have just 1 operand");
      out << "PushArg ";
      print_ir_operand(instr.operands[0], out);
      break;
    case IROpCode::LCALL: // opt_dest = LCall func_label
      _assert_nolog(instr.operands.size() == 1, "LCALL must have 1 operand");
      if (instr.result.has_value()) {
        print_ir_operand(instr.result.value(), out);
        out << " = ";
      }
      out << "LCall ";
      print_ir_operand(instr.operands[0], out);
      break;
    case IROpCode::ASM_BLOCK:
      _assert_nolog(
          !instr.result.has_value() && instr.operands.size() == 1 &&
              std::holds_alternative<std::string>(instr.operands[0]),
          "ASM_BLOCK must have just 1 operand and it must be an std::string");
      out << "Asm ";
      print_ir_operand(instr.operands[0], out);
      break;
    case IROpCode::LABEL: break;
    case IROpCode::ADDR_OF:
      _assert_nolog(instr.result.has_value() && instr.operands.size() == 1,
              "AddrOf must have a result value and 1 operand");
      print_ir_operand(instr.result.value(), out);
      out << " = AddrOf ";
      print_ir_operand(instr.operands[0], out);
      break;
    case IROpCode::ALLOC:
      _assert_nolog(instr.result.has_value() && !instr.operands.empty(),
              "Alloc must have a result value and >0 operands");
      print_ir_operand(instr.result.value(), out);
      out << " = Alloc ";
      print_ir_operand(instr.operands[0], out); // size_imm
      if (instr.operands.size() > 1) {
        out << ", ";
        print_ir_operand(instr.operands[1], out); // init_val_op
      }
      break;
    case IROpCode::ALLOC_ARRAY:
      _assert_nolog(instr.result.has_value() && instr.operands.size() == 2,
              "AllocArray must have a result value and 2 operands");
      print_ir_operand(instr.result.value(), out);
      out << " = AllocArray ";
      print_ir_operand(instr.operands[0], out); // elem_size_imm
      out << ", ";
      print_ir_operand(instr.operands[1], out); // num_elements_op
      break;
    case IROpCode::FREE:
      _assert_nolog(!instr.result.has_value() && instr.operands.size() == 1,
              "FREE must have 1 operand");
      out << "Free ";
      print_ir_operand(instr.operands[0], out);
      break;
    case IROpCode::MEM_COPY:
      _assert_nolog(instr.result.has_value() && instr.operands.size() == 1,
              "MEM_COPY must have a destination and 1 operand");
      print_ir_operand(instr.result.value(), out);
      out << " = memcpy ";
      print_ir_operand(instr.operands[0], out);
      break;
    default:
      out << "UNKNOWN_IR_OPCODE(" << static_cast<int>(instr.opcode) << ")";
      break;
  }

  out << "\n";
}

void print_ir_instructions(const std::vector<IRInstruction>& instructions,
                           std::ostream& out) {
  out << "--- IR Instructions ---\n";
  if (instructions.empty()) {
    out << "(No instructions generated)\n";
  }
  for (const IRInstruction& instr : instructions) {
    print_ir_instruction(instr, out);
  }
  out << "-----------------------\n";
}
