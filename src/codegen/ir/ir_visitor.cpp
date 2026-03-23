#include "ir_visitor.h"

#include <cstdint>

#include "../../parser/visitor.h"
#include "../../util.h"

static bool is_str_cmp(Type* a, Type* b) {
  return a->is<Type::Named>() && a->as<Type::Named>().identifier == "string" &&
         b->is<Type::Named>() && b->as<Type::Named>().identifier == "string";
}

static std::string make_ir_var_name(Variable* var) {
  return std::string(var->name) + "_" + std::to_string(var->scope_id);
}

IrVisitor::IrVisitor(const SymTab* tab, Logger* logger)
    : m_symtab(tab),
      m_logger(logger),
      m_last_expr_operand(IR_Register(-1)),
      m_main_function_defined(false) {}

const std::vector<IRInstruction>& IrVisitor::get_instructions() const {
  return m_ir_gen.get_instructions();
}

void IrVisitor::visit_all(const std::vector<AstPtr>& ast) {
  // pre-pass for function declarations to allow forward calls.
  // works with the typechecker to allow for forward declarations.
  for (const AstPtr& ast_node : ast) {
    if (FuncDeclPtr func_decl = dynamic_cast<FuncDeclPtr>(ast_node)) {
      std::string_view func_name = func_decl->name->name_str;
      if (func_name == "main") {
        m_main_function_defined = true;
      }
      // add it as a variable so that function calls can find it's identifier
      add_var(func_name, func_decl->scope_id, true, func_decl->is_extern);
    }
  }

  for (const AstPtr& ast_node : ast) {
    ast_node->accept(*this);
  }
}

IR_Label IrVisitor::get_runtime_print_call(Type* type) {
  _assert(type, "Type must be known for print function selection");
  if (type->is<Type::Named>()) {
    std::string_view name = type->as<Type::Named>().identifier;
    if (name == "string") return IR_Label("print_string");
    if (name == "bool") return IR_Label("print_int");
    if (name == "i64" || name == "u64" || name == "i32" || name == "u32" ||
        name == "i16" || name == "u16" || name == "i8" || name == "u8") {
      return IR_Label("print_int");
    }
  }
  return IR_Label("print_int");
}

bool IrVisitor::var_exists(std::string_view name, size_t scope_id) {
  Variable* var = m_symtab->lookup<Variable>(name, scope_id);
  _assert(var != nullptr, "var should exist within symbol table");
  _assert(var->type, "variable type should be resolved (even if inferred)");
  std::uint64_t size = var->type->get_byte_size();
  return m_vars.find(IR_Variable(make_ir_var_name(var), size)) != m_vars.end();
}

const IR_Variable& IrVisitor::get_var(std::string_view name, size_t scope_id) {
  Variable* var = m_symtab->lookup<Variable>(name, scope_id);
  if (!var_exists(name, scope_id)) _assert(false, "var is expected to exist: " + std::string(name));
  std::uint64_t size = var->type->get_byte_size();
  return *m_vars.find(IR_Variable(make_ir_var_name(var), size));
}

const IR_Variable* IrVisitor::add_var(std::string_view name, size_t scope_id,
                                      bool is_func_decl, bool is_extern) {
  Variable* var = m_symtab->lookup<Variable>(name, scope_id);
  if (var_exists(name, scope_id)) _assert(false, "var already exists: " + std::string(name));
  std::uint64_t size = var->type->get_byte_size();
  auto itr = m_vars.insert(IR_Variable(make_ir_var_name(var), size, is_func_decl, is_extern));
  return &(*itr.first);
}

void IrVisitor::unimpl(const std::string& note) {
  _assert(false, "Unimplemented IR feature: " + note);
}

void IrVisitor::visit(PoisonExprNode&) { /* no-op */
  _assert(false, "found a poisoned expr node in the IR");
}
void IrVisitor::visit(PoisonStmtNode&) { /* no-op */
  _assert(false, "found a poisoned expr node in the IR");
}

// Expression Nodes
void IrVisitor::visit(NullLiteralNode& node) {
  _assert(node.expr_type, "Type checker must resolve literal types");
  _assert(node.expr_type->get_byte_size() == Type::PTR_SIZE, "Null should be ptr size");
  m_last_expr_operand = IR_Immediate(0, node.expr_type->get_byte_size());
}

void IrVisitor::visit(IntegerLiteralNode& node) {
  _assert(node.expr_type, "Type checker must resolve literal types");
  m_last_expr_operand = IR_Immediate(node.value, node.expr_type->get_byte_size());
}

void IrVisitor::visit(BoolLiteralNode& node) {
  _assert(node.expr_type, "Type checker must resolve literal types");
  m_last_expr_operand = IR_Immediate(node.value ? 1 : 0, node.expr_type->get_byte_size());
}

void IrVisitor::visit(StringLiteralNode& node) {
  /* backend will interpret the string
      and assume that it is PTR_SIZE */
  m_last_expr_operand = node.value;
}

void IrVisitor::visit(CharLiteralNode& node) {
  _assert(node.expr_type, "Type checker must resolve literal types");
  m_last_expr_operand = IR_Immediate(node.value, node.expr_type->get_byte_size());
}

void IrVisitor::visit(IdentifierNode& node) {
  _assert(var_exists(node.name_str, node.scope_id),
          "Type checker should identify use of undeclared identifiers");
  m_last_expr_operand = get_var(node.name_str, node.scope_id);
}

void IrVisitor::visit(BinaryOpExprNode& node) {
  /* short-circuiting operators first */
  if (node.op_type == BinOperator::LogicalAnd || node.op_type == BinOperator::LogicalOr) {
    IR_Register dest_reg = m_ir_gen.new_temp_reg();
    IR_Label end_label = m_ir_gen.new_label();

    node.left->accept(*this);
    IROperand left_op = m_last_expr_operand;
    std::uint64_t bool_size = node.expr_type->get_byte_size();
    _assert(bool_size == 1, "LHS of binop should be 1 byte boolean");

    if (node.op_type == BinOperator::LogicalAnd) {
      // if !left, jump to end return false
      m_ir_gen.emit_assign(dest_reg, IR_Immediate(0, bool_size), bool_size);
      m_ir_gen.emit_if_z(left_op, end_label, bool_size);
    } else {
      // if left, jump to end and return true
      IR_Label eval_right_label = m_ir_gen.new_label();
      m_ir_gen.emit_assign(dest_reg, IR_Immediate(1, bool_size), bool_size);
      m_ir_gen.emit_if_z(left_op, eval_right_label, bool_size); // if left is 0, we must evaluate right
      m_ir_gen.emit_goto(end_label); // left was true, skip to end
      m_ir_gen.emit_label(eval_right_label);
    }

    // evaluate the right side (only reached if left didn't short-circuit)
    node.right->accept(*this);
    IROperand right_op = m_last_expr_operand;
    m_ir_gen.emit_assign(dest_reg, right_op, bool_size);

    m_ir_gen.emit_label(end_label);
    m_last_expr_operand = dest_reg;
    return;
  }

  /* standard binary operators */
  node.left->accept(*this);
  IROperand left_op = m_last_expr_operand;

  node.right->accept(*this);
  IROperand right_op = m_last_expr_operand;

  _assert(node.left->expr_type && node.right->expr_type,
          "Types should be resolved by Typechecker");

  IR_Register dest_reg = m_ir_gen.new_temp_reg();

  // the only case that these should/can be different sizes is if they are nums
  std::uint64_t l_size = node.left->expr_type->get_byte_size();
  std::uint64_t r_size = node.right->expr_type->get_byte_size();

  // literal values can have their sizes modified to fit the job
  if (l_size != r_size) {
    if (std::holds_alternative<IR_Immediate>(right_op)) {
      r_size = l_size;
    } else if (std::holds_alternative<IR_Immediate>(left_op)) {
      l_size = r_size;
    }
  }

  _assert(l_size == r_size, "Operands of a binary operation must have matching sizes in IR generation (casting should be performed prior via an AST CastNode)");

  uint64_t op_size = l_size;

  switch (node.op_type) {
    case BinOperator::Plus:
      if (is_str_cmp(node.left->expr_type, node.right->expr_type)) {
        m_ir_gen.emit_begin_lcall_prep();
        m_ir_gen.emit_push_arg(left_op, Type::PTR_SIZE);
        m_ir_gen.emit_push_arg(right_op, Type::PTR_SIZE);
        m_ir_gen.emit_lcall(dest_reg, IR_Label("string_concat"), Type::PTR_SIZE);
      } else {
        m_ir_gen.emit_add(dest_reg, left_op, right_op, op_size);
      }
      break;
    case BinOperator::Minus:
      m_ir_gen.emit_sub(dest_reg, left_op, right_op, op_size);
      break;
    case BinOperator::Multiply:
      m_ir_gen.emit_mul(dest_reg, left_op, right_op, op_size);
      break;
    case BinOperator::Divide:
      m_ir_gen.emit_div(dest_reg, left_op, right_op, op_size);
      break;
    case BinOperator::Equal:
      if (is_str_cmp(node.left->expr_type, node.right->expr_type)) {
        m_ir_gen.emit_cmp_str_eq(dest_reg, left_op, right_op, Type::PTR_SIZE);
      } else {
        m_ir_gen.emit_cmp_eq(dest_reg, left_op, right_op, op_size);
      }
      break;
    case BinOperator::NotEqual:
      if (is_str_cmp(node.left->expr_type, node.right->expr_type)) {
        m_ir_gen.emit_cmp_str_eq(dest_reg, left_op, right_op, Type::PTR_SIZE);
        m_ir_gen.emit_log_not(dest_reg, dest_reg, 1); // one bit for str_eqs
      } else {
        m_ir_gen.emit_cmp_ne(dest_reg, left_op, right_op, op_size);
      }
      break;
    case BinOperator::LessThan:
      m_ir_gen.emit_cmp_lt(dest_reg, left_op, right_op, op_size);
      break;
    case BinOperator::LessEqual:
      m_ir_gen.emit_cmp_le(dest_reg, left_op, right_op, op_size);
      break;
    case BinOperator::GreaterThan:
      m_ir_gen.emit_cmp_gt(dest_reg, left_op, right_op, op_size);
      break;
    case BinOperator::GreaterEqual:
      m_ir_gen.emit_cmp_ge(dest_reg, left_op, right_op, op_size);
      break;
    case BinOperator::Modulo:
      m_ir_gen.emit_mod(dest_reg, left_op, right_op, op_size);
      break;
    case BinOperator::BitwiseAnd:
      m_ir_gen.emit_bit_and(dest_reg, left_op, right_op, op_size);
      break;
    case BinOperator::BitwiseOr:
      m_ir_gen.emit_bit_or(dest_reg, left_op, right_op, op_size);
      break;
    case BinOperator::BitwiseXor:
      m_ir_gen.emit_bit_xor(dest_reg, left_op, right_op, op_size);
      break;
    case BinOperator::ShiftLeft:
      m_ir_gen.emit_shl(dest_reg, left_op, right_op, op_size);
      break;
    case BinOperator::ShiftRight:
      m_ir_gen.emit_shr(dest_reg, left_op, right_op, op_size);
      break;
    default:
      _assert(false, "Unhandled binary operator in IR generation");
      break;
  }

  m_last_expr_operand = dest_reg;
}

void IrVisitor::visit(UnaryExprNode& node) {
  node.operand->accept(*this);
  IROperand object = m_last_expr_operand;

  IR_Register dest_reg = m_ir_gen.new_temp_reg();

  _assert(node.operand->expr_type, "Types should be resolved by Typechecker");
  std::uint64_t size = node.operand->expr_type->get_byte_size();
  switch (node.op_type) {
    case UnaryOperator::Negate:
      // dest = -operand
      m_ir_gen.emit_neg(dest_reg, object, size);
      break;
    case UnaryOperator::LogicalNot:
      m_ir_gen.emit_log_not(dest_reg, object, size);
      break;
    case UnaryOperator::Dereference:
      m_ir_gen.emit_load(dest_reg, object, node.expr_type->get_byte_size());
      break;
    case UnaryOperator::AddressOf:
    case UnaryOperator::AddressOfMut:
      visit_addrof(node.operand, dest_reg);
      break;
  }

  m_last_expr_operand = dest_reg;
}

void IrVisitor::visit_addrof(const ExprPtr& op, const IR_Register& dst) {
  if (IdentPtr ident_node = dynamic_cast<IdentPtr>(op)) {
    m_ir_gen.emit_addr_of(dst, get_var(ident_node->name_str, ident_node->scope_id));
  } else if (ArrayIndexNode* array_idx_node = dynamic_cast<ArrayIndexNode*>(op)) {
    // calculate address of arr[idx] and store in dest_reg
    array_idx_node->object->accept(*this);
    IROperand base_addr_op = m_last_expr_operand;

    array_idx_node->index->accept(*this);
    IROperand index_op = m_last_expr_operand;

    _assert(array_idx_node->expr_type && array_idx_node->index->expr_type,
        "node and index should have a resolved type so that we can size it");

    std::uint64_t pointee_type_size = array_idx_node->expr_type->get_byte_size();
    IR_Immediate elem_size_imm(pointee_type_size, Type::PTR_SIZE);

    IR_Register offset_reg = m_ir_gen.new_temp_reg();
    m_ir_gen.emit_mul(offset_reg, index_op, elem_size_imm, Type::PTR_SIZE);

    m_ir_gen.emit_add(dst, base_addr_op, offset_reg, Type::PTR_SIZE);
  } else if (UnaryExprNode* unary_deref_node = dynamic_cast<UnaryExprNode*>(op)) {
    if (unary_deref_node->op_type == UnaryOperator::Dereference) {
      unary_deref_node->operand->accept(*this);
      m_ir_gen.emit_assign(dst, m_last_expr_operand, Type::PTR_SIZE);
    } else {
      unimpl("AddressOf non-dereference UnaryExpr for lvalue: " +
             AstPrinter::unary_op_to_string(unary_deref_node->op_type));
    }
  } else if (FieldAccessNode* field_node = dynamic_cast<FieldAccessNode*>(op)) {
    IR_Register field_addr = compute_struct_field_addr(field_node->object, field_node->field->name_str);
    m_ir_gen.emit_assign(dst, field_addr, Type::PTR_SIZE);
  } else if (dynamic_cast<StructLiteralNode*>(op)) {
    // struct literals allocate heap memory and return their pointer directly
    op->accept(*this);
    m_ir_gen.emit_assign(dst, m_last_expr_operand, Type::PTR_SIZE);
  } else {
    unimpl("AddressOf complex l-value");
  }
}

IR_Register IrVisitor::compute_struct_field_addr(const ExprPtr& object, std::string_view field_name) {
  Type* obj_type = object->expr_type;
  Type* struct_type = nullptr;

  // type of the base is either a struct or pointer to a struct
  if (obj_type->is<Type::Pointer>()) {
    struct_type = obj_type->as<Type::Pointer>().pointee;
  } else {
    struct_type = obj_type;
  }

  _assert(struct_type->is<Type::Named>(), "Field access on non-struct");

  std::string name = struct_type->to_string();
  size_t scope_id = struct_type->get_scope_id();
  StructDeclPtr struct_decl = m_symtab->lookup_struct(name, scope_id);
  _assert(struct_decl, "Struct definition not found");

  // find the field offset
  std::uint64_t offset = 0;
  bool found = false;
  for (const StructFieldPtr& field : struct_decl->fields) {
    if (field->name->name_str == field_name) {
      found = true;
      break;
    }
    offset += field->type->get_byte_size();
  }
  _assert(found, "Struct field not found");

  // get base address of struct
  object->accept(*this); // it points to base addr
  IROperand  base_addr_op = m_last_expr_operand;

  // final address: base + offset
  IR_Register addr_reg = m_ir_gen.new_temp_reg();
  IR_Immediate off_imm(offset, Type::PTR_SIZE);
  m_ir_gen.emit_add(addr_reg, base_addr_op, off_imm, Type::PTR_SIZE);

  return addr_reg;
}

void IrVisitor::visit(FieldAccessNode& node) {
  // compute the memory address of the field
  IR_Register field_addr = compute_struct_field_addr(node.object, node.field->name_str);

  _assert(node.expr_type, "FieldAccessNode must have its type resolved by typechecker");
  std::uint64_t size = node.expr_type->get_byte_size();

  // load the value from the computed address
  IR_Register result_reg = m_ir_gen.new_temp_reg();
  m_ir_gen.emit_load(result_reg, field_addr, size);
  m_last_expr_operand = result_reg;
}

void IrVisitor::visit(AssignmentNode& node) {
  node.rvalue->accept(*this);
  Type* lval_type = node.lvalue->expr_type;

  // Assignment is defaultly a copy; handle string/struct copying semantics
  ExprPtr rval = node.rvalue;
  IROperand rval_op = get_copy_of_operand(m_last_expr_operand, rval);

  _assert(lval_type, "Types should be resolved by Typechecker");
  std::uint64_t size = lval_type->get_byte_size();

  // Handle all possible L-values and stores
  if (auto lval = dynamic_cast<IdentifierNode*>(node.lvalue)) {
    _assert(var_exists(lval->name_str, lval->scope_id),
            "Type checker should identify use of undeclared variable");
    IR_Variable lval_var = get_var(lval->name_str, lval->scope_id);
    m_ir_gen.emit_assign(lval_var, rval_op, size);
  } else {
    // handles all complex l-values (array-indexing, struct field access, dereferencing, etc)
    IR_Register target_addr_reg = m_ir_gen.new_temp_reg();
    // compute memory address and store our value
    visit_addrof(node.lvalue, target_addr_reg);
    m_ir_gen.emit_store(target_addr_reg, rval_op, size);
  }

  m_last_expr_operand = rval_op;
}

void IrVisitor::visit(ImplicitCastNode& node) {
  node.expression->accept(*this);
  IROperand src_op = m_last_expr_operand;
  Type* dst_type = node.target_type;

  std::uint64_t dst_size = dst_type->get_byte_size();

  // if it's a literal, just change the size
  if (std::holds_alternative<IR_Immediate>(src_op)) {
    IR_Immediate imm = std::get<IR_Immediate>(src_op);
    m_last_expr_operand = IR_Immediate(imm.val, dst_size);
    return;
  }

  IR_Register dest_reg = m_ir_gen.new_temp_reg();
  m_ir_gen.emit_assign(dest_reg, src_op, dst_size);
  m_last_expr_operand = dest_reg;
}

void IrVisitor::visit(GroupedExprNode& node) { node.expression->accept(*this); }

void IrVisitor::visit(ExpressionStatementNode& node) {
  node.expression->accept(*this);
}

// Statement Nodes
void IrVisitor::visit(VariableDeclNode& node) {
  _assert(node.scope_id == node.name->scope_id,
          "variable scope id should be the same as variable name scope id");
  _assert(!var_exists(node.name->name_str, node.scope_id),
          "Type checker should identify variable re-declaration");

  size_t var_scope = node.scope_id;
  std::string_view var_name = node.name->name_str;
  const IR_Variable* var_operand = add_var(var_name, var_scope);

  _assert(node.type != nullptr, "Variable type must be resolved by the typechecker");
  std::uint64_t size = node.type->get_byte_size();

  if (node.initializer) {
    node.initializer->accept(*this);
    bool str_lhs_mut = is_string_mutable(node.type, std::string(var_name), var_scope);
    IROperand res = get_copy_of_operand(m_last_expr_operand, node.initializer, str_lhs_mut);
    m_ir_gen.emit_assign(*var_operand, res, size);
  } else {
    // default values, right now just assign 0
    m_ir_gen.emit_assign(*var_operand, IR_Immediate(0, size), size);
  }
}

void IrVisitor::visit(IfStmtNode& node) {
  node.condition->accept(*this);
  IROperand cond_op = m_last_expr_operand;

  IR_Label else_label = m_ir_gen.new_label();
  IR_Label end_label = m_ir_gen.new_label();

  _assert(node.condition->expr_type, "Types should be resolved by Typechecker");
  m_ir_gen.emit_if_z(cond_op, else_label,
                     node.condition->expr_type->get_byte_size());

  node.then_branch->accept(*this);

  if (node.else_branch) {
    m_ir_gen.emit_goto(end_label);
  }

  m_ir_gen.emit_label(else_label);

  if (node.else_branch) {
    node.else_branch->accept(*this);
    m_ir_gen.emit_label(end_label);
  }
  // else the else_branch serves as the end_label
}

void IrVisitor::visit(WhileStmtNode& node) {
  IR_Label start_loop_label = m_ir_gen.new_label();
  IR_Label end_loop_label = m_ir_gen.new_label();

  m_loop_contexts.push({start_loop_label, end_loop_label});

  m_ir_gen.emit_label(start_loop_label);

  node.condition->accept(*this);
  IROperand cond_op = m_last_expr_operand;

  _assert(node.condition->expr_type, "Types should be resolved by Typechecker");
  m_ir_gen.emit_if_z(cond_op, end_loop_label,
                     node.condition->expr_type->get_byte_size());

  node.body->accept(*this);

  m_ir_gen.emit_goto(start_loop_label);
  m_ir_gen.emit_label(end_loop_label);

  m_loop_contexts.pop();
}

void IrVisitor::visit(BreakStmtNode&) {
  _assert(!m_loop_contexts.empty(), "break stmt outside of loop context in IR");
  m_ir_gen.emit_goto(m_loop_contexts.top().second); // break_target
}

void IrVisitor::visit(ContinueStmtNode&) {
  _assert(!m_loop_contexts.empty(), "continue stmt outside of loop context in IR");
  m_ir_gen.emit_goto(m_loop_contexts.top().first); // continue_target
}

void IrVisitor::visit(BlockNode& node) {
  for (const StmtPtr& stmt : node.statements) {
    stmt->accept(*this);
  }
}

void IrVisitor::visit(FunctionDeclNode& node) {
  if (node.is_extern) return; // it is already in asm runtime-library

  std::string original_func_name = std::string(node.name->name_str);
  std::string func_ir_name = original_func_name;

  if (original_func_name != "main") {
    func_ir_name = "_" + original_func_name;
  }

  IR_Label func_label = m_ir_gen.new_func_label(func_ir_name);

  m_ir_gen.emit_begin_func(func_label);

  // allocate registers for arguments
  std::vector<IR_Variable> ordered_param_vars;
  for (const ParamPtr& param_node : node.params) {
    param_node->accept(*this); // inserts into m_vars
    ordered_param_vars.push_back(
        get_var(param_node->name->name_str, param_node->scope_id));
  }

  // emit assignments from argument slots to parameter variables so that the
  // code generation can load the passed arguments onto the stack as local vars
  size_t p_amt = ordered_param_vars.size();
  for (size_t i = 0; i < p_amt; ++i) {
    const IR_Variable& param_var = ordered_param_vars[i];
    std::uint64_t param_size = node.params[i]->type->get_byte_size();
    m_ir_gen.emit_assign(param_var, IR_ParameterSlot(i, p_amt, param_size),
                         param_size);
  }

  IR_Label epilogue_label = m_ir_gen.new_label();
  std::optional<IR_Label> prev_epilogue_label = m_current_func_epilogue_label;
  m_current_func_epilogue_label = epilogue_label;

  if (node.retvar_name) {
    add_var(node.retvar_name->name_str, node.body->scope_id);
  }

  if (node.body) {
    node.body->accept(*this);
  }

  // setup void return before epilogue
  if (node.retvar_name) {
    // named return, defaultly return the value within the return variable
    IROperand named_operand = get_var(node.retvar_name->name_str, node.retvar_name->scope_id);
    m_ir_gen.emit_return(named_operand, node.return_type->get_byte_size());
  } else {
    // void return, add default return before epilogue in case there are no
    // explicit return statements in the code.
    m_ir_gen.emit_return();
  }
  m_ir_gen.emit_label(epilogue_label);
  m_ir_gen.emit_end_func();

  m_current_func_epilogue_label = prev_epilogue_label;
}

void IrVisitor::visit(ArgumentNode& node) {
  // Much later on we will be include handling of different forms of copying, vs
  // 'give'ing and 'take'ing. For now, just accept from the expression.
  node.expression->accept(*this);
}

void IrVisitor::visit(ParamNode& node) {
  add_var(node.name->name_str, node.scope_id);
}

void IrVisitor::visit(FunctionCallNode& node) {
  _assert(node.callee->expr_type, "Callee must have a type");
  _assert(node.callee->expr_type->is<Type::Function>(),
          "Callee in a function call must have a function type");
  const Type::Function& func_type = node.callee->expr_type->as<Type::Function>();

  /* evaluate callee to prevent argument clobbering (fn ptrs) */
  node.callee->accept(*this);
  IROperand func_target = m_last_expr_operand;

  m_ir_gen.emit_begin_lcall_prep();
  for (size_t i = 0; i < node.arguments.size(); ++i) {
    ArgPtr arg_node = node.arguments[i];
    arg_node->accept(*this);
    IROperand arg_op = m_last_expr_operand;

    ExprPtr arg_expr = arg_node->expression;
    _assert(arg_expr->expr_type, "Types should be resolved by Typechecker");

    const Type::ParamInfo& param_info = func_type.params[i];
    IROperand final_arg_op = arg_op;

    if (param_info.modifier == BorrowState::ImmutablyOwned ||
        param_info.modifier == BorrowState::MutablyOwned) { // take
      if (arg_node->is_give) {
        bool str_mut = is_string_mutable(param_info.type, param_info.modifier);
        final_arg_op = get_copy_of_operand(arg_op, arg_expr, str_mut);
      }
    }

    Type* arg_type = arg_expr->expr_type;
    m_ir_gen.emit_push_arg(final_arg_op, arg_type->get_byte_size());
  }

  std::optional<IR_Register> result_reg_opt = std::nullopt;

  // if it is not void, prepare a temp register for return value
  _assert(node.expr_type, "Type checker should resolve function call");
  if (!(node.expr_type->is<Type::Named>() &&
        node.expr_type->as<Type::Named>().identifier == "u0")) {
    IR_Register call_result_reg = m_ir_gen.new_temp_reg();
    result_reg_opt = call_result_reg;
    m_last_expr_operand = call_result_reg;
  } else {
    // void placeholder
    m_last_expr_operand = IR_Immediate(0, Type::PTR_SIZE);
  }

  size_t byte_size = node.expr_type->get_byte_size();
  m_ir_gen.emit_lcall(result_reg_opt, func_target, byte_size);
}

void IrVisitor::visit(ReturnStmtNode& node) {
  _assert(m_current_func_epilogue_label.has_value(),
          "Return statement found outside of a function context in IR visitor");

  if (node.value) {
    node.value->accept(*this);
    _assert(node.value->expr_type, "Types should be resolved by Typechecker");
    m_ir_gen.emit_return(m_last_expr_operand,
                         node.value->expr_type->get_byte_size());
  } else {
    // void
    m_ir_gen.emit_return();
  }
  m_ir_gen.emit_goto(m_current_func_epilogue_label.value());
}

void IrVisitor::visit(ForStmtNode& node) {
  // (initializer; condition; iteration) body
  if (node.initializer.has_value()) {
    std::visit(
        [this](auto&& arg) {
          if (arg) arg->accept(*this);
        },
        node.initializer.value());
  }

  IR_Label condition_label = m_ir_gen.new_label();
  IR_Label iteration_label = m_ir_gen.new_label();
  IR_Label end_loop_label = m_ir_gen.new_label();

  // 'continue': jump to iteration_label.
  // 'break': jump to end_loop_label.
  m_loop_contexts.push({iteration_label, end_loop_label});

  m_ir_gen.emit_label(condition_label);
  if (node.condition) {
    node.condition->accept(*this);
    _assert(node.condition->expr_type,
            "Types should be resolved by Typechecker");
    m_ir_gen.emit_if_z(m_last_expr_operand, end_loop_label,
                       node.condition->expr_type->get_byte_size());
  }

  node.body->accept(*this);

  m_ir_gen.emit_label(iteration_label);
  if (node.iteration) {
    node.iteration->accept(*this);
  }
  m_ir_gen.emit_goto(condition_label);

  m_ir_gen.emit_label(end_loop_label);
  m_loop_contexts.pop();
}

static bool is_unsigned_int(Type* type) {
  if (!type->is<Type::Named>()) return false;
  std::string_view n = type->as<Type::Named>().identifier;
  return n == "u8" || n == "u16" || n == "u32" || n == "u64" || n == "bool";
}

static bool is_signed_int(Type* type) {
  if (!type->is<Type::Named>()) return false;
  std::string_view n = type->as<Type::Named>().identifier;
  return n == "i8" || n == "i16" || n == "i32" || n == "i64";
}

void IrVisitor::visit(ReadStmtNode& node) {
  Type* expr_type = node.expression->expr_type;
  _assert(expr_type, "Read expression must have a type from typechecker");

  m_ir_gen.emit_begin_lcall_prep();
  IR_Register word_reg = m_ir_gen.new_temp_reg();
  m_ir_gen.emit_lcall(word_reg, IR_Label("read_word"), Type::PTR_SIZE);

  IR_Register val_reg = word_reg;

  // parse word into integer
  if (is_unsigned_int(expr_type) || is_signed_int(expr_type)) {
    m_ir_gen.emit_begin_lcall_prep();
    m_ir_gen.emit_push_arg(word_reg, Type::PTR_SIZE);

    val_reg = m_ir_gen.new_temp_reg();
    if (is_unsigned_int(expr_type)) {
      m_ir_gen.emit_lcall(val_reg, IR_Label("parse_uint"), 8);
    } else {
      m_ir_gen.emit_lcall(val_reg, IR_Label("parse_int"), 8);
    }
  }

  if (auto ident_node = dynamic_cast<IdentifierNode*>(node.expression)) {
    IR_Variable lval_var = get_var(ident_node->name_str, ident_node->scope_id);
    m_ir_gen.emit_assign(lval_var, val_reg, expr_type->get_byte_size());
  } else {
    // for arr[i], *ptr, struct.field
    IR_Register target_addr_reg = m_ir_gen.new_temp_reg();
    visit_addrof(node.expression, target_addr_reg);
    m_ir_gen.emit_store(target_addr_reg, val_reg, expr_type->get_byte_size());
  }
}

void IrVisitor::visit(PrintStmtNode& node) {
  for (size_t i = 0; i < node.expressions.size(); ++i) {
    node.expressions[i]->accept(*this);
    IROperand val_op = m_last_expr_operand;

    Type* expr_type = node.expressions[i]->expr_type;
    _assert(expr_type, "Print expression must have a type from typechecker");

    IR_Label print_func_lbl = get_runtime_print_call(expr_type);

    m_ir_gen.emit_begin_lcall_prep();
    // load file descriptor for print (1: stdout)
    m_ir_gen.emit_push_arg(IR_Immediate(1, Type::PTR_SIZE), Type::PTR_SIZE);
    m_ir_gen.emit_push_arg(val_op, expr_type->get_byte_size());
    m_ir_gen.emit_lcall(std::nullopt, print_func_lbl, 0);
  }
}

void IrVisitor::visit(AsmBlockNode& node) {
  m_ir_gen.emit_asm_block(node.body);
}

// R-value access: x = arr[i]
void IrVisitor::visit(ArrayIndexNode& node) {
  node.object->accept(*this); // base addr
  IROperand base_addr_op = m_last_expr_operand;

  node.index->accept(*this);
  IROperand index_op = m_last_expr_operand;

  _assert(node.expr_type, "ArrayIndexNode must have its element type resolved");

  std::uint64_t size = node.expr_type->get_byte_size();
  IR_Immediate size_op(size, Type::PTR_SIZE);

  IR_Register offset_reg = m_ir_gen.new_temp_reg();
  m_ir_gen.emit_mul(offset_reg, index_op, size_op, Type::PTR_SIZE);

  IR_Register target_addr_reg = m_ir_gen.new_temp_reg();
  m_ir_gen.emit_add(target_addr_reg, base_addr_op, offset_reg, Type::PTR_SIZE);

  IR_Register result_val_reg = m_ir_gen.new_temp_reg();
  m_ir_gen.emit_load(result_val_reg, target_addr_reg, size);
  m_last_expr_operand = result_val_reg;
}

void IrVisitor::visit(ExitStmtNode& node) {
  if (node.exit_code) {
    node.exit_code->accept(*this);
    _assert(node.exit_code->expr_type, "Types should be resolved by Typechecker");
    m_ir_gen.emit_exit(m_last_expr_operand,
        node.exit_code->expr_type->get_byte_size());
  } else {
    /* default exit code of 0 */
    m_ir_gen.emit_exit(IR_Immediate(0, Type::PTR_SIZE), Type::PTR_SIZE);
  }
}

void IrVisitor::visit(SwitchStmtNode& node) {
  node.expression->accept(*this);
  IROperand switch_op = m_last_expr_operand;
  _assert(node.expression->expr_type, "Switch expression must have a type");
  std::uint64_t expr_size = node.expression->expr_type->get_byte_size();

  CasePtr default_case_node = nullptr;
  IR_Label default_body_label;

  std::vector<std::pair<CasePtr, IR_Label>> case_blocks;

  for (const CasePtr& case_ptr : node.cases) {
    if (case_ptr->value) {
      // it is a non-default case
      IR_Label body_label = m_ir_gen.new_label();
      case_blocks.push_back(std::make_pair(case_ptr, body_label));
    } else {
      // the default case
      default_case_node = case_ptr;
      default_body_label = m_ir_gen.new_label();
    }
  }

  IR_Label end_switch_label = m_ir_gen.new_label();
  IR_Label final_fallthrough_label =
      default_case_node ? default_body_label : end_switch_label;

  // emit the cases
  for (const auto& [case_ptr, body_label] : case_blocks) {
    case_ptr->value->accept(*this);
    IROperand case_val_op = m_last_expr_operand;
    IR_Register cmp_reg = m_ir_gen.new_temp_reg();

    // do a not-equal comparison so that we can jump to the case label faster
    m_ir_gen.emit_cmp_ne(cmp_reg, switch_op, case_val_op, expr_size);

    // jump to case label if it was not (not-equal)
    m_ir_gen.emit_if_z(cmp_reg, body_label, 1); // cmp_reg is 1 byte bool
  }

  // goto either default case (if one exists) or the exit
  m_ir_gen.emit_goto(final_fallthrough_label);

  for (const auto& [case_ptr, body_label] : case_blocks) {
    m_ir_gen.emit_label(body_label);
    case_ptr->body->accept(*this);
    m_ir_gen.emit_goto(end_switch_label);
  }

  if (default_case_node) {
    m_ir_gen.emit_label(default_body_label);
    default_case_node->body->accept(*this);
    // no need to emit_goto(end_switch_label) because we can just fall-through
  }

  m_ir_gen.emit_label(end_switch_label);
}

void IrVisitor::visit(NewExprNode& node) {
  _assert(node.type_to_allocate, "NewExprNode must have a type to allocate");
  _assert(node.expr_type && node.expr_type->is<Type::Pointer>(),
          "NewExprNode result must be a pointer type");

  IR_Register result_ptr_reg = m_ir_gen.new_temp_reg();

  if (node.is_array_allocation) {
    // new <T>[size_expr]
    _assert(node.allocation_specifier,
            "Array allocation must have a size specifier");
    node.allocation_specifier->accept(*this);
    IROperand size_op = m_last_expr_operand;

    _assert(node.allocation_specifier->expr_type, "Array size specifier must have a type");
    std::uint64_t size_op_type_size =
        node.allocation_specifier->expr_type->get_byte_size();

    m_ir_gen.emit_alloc_array(result_ptr_reg,
                              node.type_to_allocate->get_byte_size(), size_op,
                              size_op_type_size);
  } else {
    // new <T>(init_expr) or new <T>()
    std::optional<IROperand> init_op_opt = std::nullopt;
    std::uint64_t initializer_type_size = 0;

    if (node.allocation_specifier) {
      node.allocation_specifier->accept(*this);
      init_op_opt = m_last_expr_operand;
      _assert(node.allocation_specifier->expr_type, "Initializer for new must have a type");
      initializer_type_size = node.allocation_specifier->expr_type->get_byte_size();
    }
    m_ir_gen.emit_alloc(result_ptr_reg, node.type_to_allocate->get_byte_size(),
                        init_op_opt, initializer_type_size);
  }
  m_last_expr_operand = result_ptr_reg;
}

void IrVisitor::visit(FreeStmtNode& node) {
  node.expression->accept(*this);
  IROperand ptr_to_free_op = m_last_expr_operand;
  // right now we treat `node.is_array_deallocation` and normal, the same
  m_ir_gen.emit_free(ptr_to_free_op);
}

// same as PrintStmtNode but printing with fd = 2
void IrVisitor::visit(ErrorStmtNode& node) {
  for (size_t i = 0; i < node.expressions.size(); ++i) {
    node.expressions[i]->accept(*this);
    IROperand val_op = m_last_expr_operand;

    Type* expr_type = node.expressions[i]->expr_type;
    _assert(expr_type, "ErrorPrint expression must have a type from typechecker");

    IR_Label print_func_lbl = get_runtime_print_call(expr_type);

    m_ir_gen.emit_begin_lcall_prep();
    // load file descriptor for print (2: stderr)
    m_ir_gen.emit_push_arg(IR_Immediate(2, Type::PTR_SIZE), Type::PTR_SIZE);
    m_ir_gen.emit_push_arg(val_op, expr_type->get_byte_size());
    m_ir_gen.emit_lcall(std::nullopt, print_func_lbl, 0);
  }
}

void IrVisitor::visit(FloatLiteralNode&) { unimpl("FloatLiteralNode"); }

void IrVisitor::visit(StructFieldNode&) { /* no-op */ }
void IrVisitor::visit(StructDeclNode&) { /* no-op */ }
void IrVisitor::visit(StructFieldInitializerNode&) { /* no-op */ }

void IrVisitor::visit(StructLiteralNode& node) {
  // allocate memory for the struct
  std::uint64_t struct_size = node.struct_decl->struct_size;
  IR_Register struct_addr = m_ir_gen.new_temp_reg();
  m_ir_gen.emit_alloc(struct_addr, struct_size, std::nullopt, 0);

  // for each field initializer, store the value at the correct offset
  size_t offset = 0;
  for (const StructFieldPtr& field : node.struct_decl->fields) {
    // find the initializer for this field
    std::string_view field_name = field->name->name_str;
    ExprPtr value = nullptr;
    for (const StructFieldInitPtr& init : node.initializers) {
      if (init->field->name_str == field_name) {
        value = init->value;
        break;
      }
    }

    // store at struct_addr + offset
    IR_Register field_addr = m_ir_gen.new_temp_reg();
    m_ir_gen.emit_add(field_addr, struct_addr,
                      IR_Immediate(offset, Type::PTR_SIZE), Type::PTR_SIZE);

    if (value == nullptr) {
      // the struct field initializer is missing for one of the fields
      m_logger->report(Diag::Warning(node.token->get_span(), "missing struct field initializer: " + std::string(field_name)));

      // zero-initialize the memory as a fallback
      m_ir_gen.emit_store(field_addr, IR_Immediate(0, field->type->get_byte_size()), field->type->get_byte_size());
    } else {
      value->accept(*this);
      IROperand val = m_last_expr_operand;
      m_ir_gen.emit_store(field_addr, val, field->type->get_byte_size());
    }
    offset += field->type->get_byte_size();
  }
  // the result of the struct literal is the address
  m_last_expr_operand = struct_addr;
}

IROperand IrVisitor::get_copy_of_operand(const IROperand& src, ExprPtr rhs_expr, bool is_str_lhs_mut) {
  Type* rhs_type = rhs_expr->expr_type;
  if (rhs_type->is<Type::Named>()) {
    // check if it is a struct
    if (dynamic_cast<StructLiteralNode*>(rhs_expr)) {
      return src; // we won't be copying a struct literal, it is redundant
    }

    std::string_view name = rhs_type->as<Type::Named>().identifier;
    size_t scope = rhs_type->get_scope_id();
    StructDeclPtr struct_decl = m_symtab->lookup_struct(name, scope);

    if (struct_decl != nullptr) {
      // it must be a struct
      std::uint64_t struct_size = struct_decl->struct_size;
      IR_Register new_addr_reg = m_ir_gen.new_temp_reg();
      m_ir_gen.emit_alloc(new_addr_reg, struct_size, std::nullopt, 0);
      m_ir_gen.emit_mem_copy(new_addr_reg, src, struct_size);
      return new_addr_reg;
    }

    // check if it is a string
    if (name == "string") {
      if (is_str_lhs_mut || !is_string_literal_expr(rhs_expr)) {
        // heap-allocate if the l-value is mutable, or it is not a str literal
        IR_Register result_reg = m_ir_gen.new_temp_reg();
        m_ir_gen.emit_begin_lcall_prep();
        m_ir_gen.emit_push_arg(src, Type::PTR_SIZE);
        m_ir_gen.emit_lcall(result_reg, IR_Label("string_copy"), Type::PTR_SIZE);
        return result_reg;
      } else {
        return src; // return reference
      }
    }
  }
  // default: primitive types, pointers (shallow copy), return src itself
  return src;
}

bool IrVisitor::is_string_mutable(Type* type, const std::string& name, size_t scope_id) {
  bool is_string_mut = type->is<Type::Named>() && type->as<Type::Named>().identifier == "string";
  if (is_string_mut) {
    Variable* var_sym = m_symtab->lookup<Variable>(name, scope_id);
    is_string_mut =
        var_sym && (var_sym->modifier == BorrowState::MutablyOwned ||
                    var_sym->modifier == BorrowState::MutablyBorrowed);
  }
  return is_string_mut;
}

bool IrVisitor::is_string_mutable(Type* type, BorrowState modifier) {
  return type->is<Type::Named>() &&
         type->as<Type::Named>().identifier == "string" &&
         (modifier == BorrowState::MutablyOwned ||
          modifier == BorrowState::MutablyBorrowed);
}

bool IrVisitor::is_string_literal_expr(const ExprPtr& expr) {
  return dynamic_cast<StringLiteralNode*>(expr) != nullptr;
}
