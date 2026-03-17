#ifndef CODEGEN_IR_IR_PRINTER_H
#define CODEGEN_IR_IR_PRINTER_H

#include <iostream>
#include <vector>

#include "ir_instruction.h"

void print_ir_operand(const IROperand& operand, std::ostream& out);
void print_ir_register(const IR_Register& reg, std::ostream& out);
void print_ir_variable(const IR_Variable& var, std::ostream& out);
void print_ir_parameter_slot(const IR_ParameterSlot& slot, std::ostream& out);
void print_ir_immediate(const IR_Immediate& imm, std::ostream& out);
void print_ir_label(const IR_Label& label, std::ostream& out);
void print_ir_instruction(const IRInstruction& instr, std::ostream& out);
void print_ir_instructions(const std::vector<IRInstruction>& instructions, std::ostream& out);

#endif // CODEGEN_IR_IR_PRINTER_H
