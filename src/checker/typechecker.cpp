#include "typechecker.h"

#include <algorithm>
#include <unordered_set>

#include "../logging/diagnostic.h"
#include "../parser/types.h"

/* we utilize the poisoned nodes to stop checking and return ErrorType,
   without every logging new errors to avoid duplicate errors */
/* every visited node should have a resolved type, if it is poisoned, it will be ErrorType */

#define _throw_internal(msg) throw std::runtime_error(std::string("Internal TypeChecker Error: ") + msg)
#define _assert(cond, msg)  \
  do {                      \
    if (!(cond)) {          \
      _throw_internal(msg); \
    }                       \
  } while (0)

bool TypeChecker::is_integer_type(Type* type) const {
  if (!type || !type->is<Type::Named>()) return false;
  const std::string_view& name = type->as<Type::Named>().identifier;
  return name == "i8" || name == "i16" || name == "i32" || name == "i64" ||
         name == "u8" || name == "u16" || name == "u32" || name == "u64";
}

bool TypeChecker::is_numeric_type(Type* type) const {
  if (!type) return false;
  if (is_integer_type(type)) return true;
  if (type->is<Type::Named>()) {
    return type->as<Type::Named>().identifier == "f64";
  }
  return false;
}

bool TypeChecker::is_primitive_type(Type* type) const {
  if (!type || !type->is<Type::Named>()) return false;
  return is_numeric_type(type) ||
         type->as<Type::Named>().identifier == "string" ||
         type->as<Type::Named>().identifier == "bool";
}

TypeChecker::TypeChecker(Logger* logger, ArenaAllocator* arena)
    : m_logger(logger),
      m_arena(arena),
      m_symtab(nullptr),
      m_current_function_return_type(nullptr),
      m_in_loop(false) {}

void TypeChecker::check_program(SymTab* symtab, const std::vector<AstPtr>& program_nodes) {
  m_symtab = symtab;
  m_current_function_return_type = nullptr;
  m_in_loop = false;

  for (const AstPtr& node : program_nodes) {
    if (node) {
      try {
        node->accept(*this);
      } catch (const std::runtime_error& internal_error) {
        m_logger->report(Diag::Fatal(node->token->get_span(), internal_error.what()));
        throw;
      }
    }
  }
}

Type* TypeChecker::get_expr_type(const ExprPtr& expr) {
  _assert(expr != nullptr, "ExprPtr should not be null (poisoned at least)");
  expr->accept(*this); // resolve type
  return expr->expr_type;
}

bool TypeChecker::check_type_assignable(Type* target_type, Type* value_type, const Span& span) {
  _assert(target_type && value_type, "assignable types should be non-null");

  // avoid cascading errors
  if (target_type->is<Type::ErrorType>() || value_type->is<Type::ErrorType>()) {
      return true;
  }

  // Check for exact type match, with number casting allowed
  if (*target_type == *value_type ||
      (is_numeric_type(target_type) && is_numeric_type(value_type))) {
    return true;
  }

  // allow assigning 'null' to any pointer type
  if (target_type->is<Type::Pointer>() && value_type->is<Type::Named>() &&
      value_type->as<Type::Named>().identifier == "u0") {
    return true;
  }

  /*
  Pointer compatibility:
     ptr<[mut] T> can be assigned ptr<[mut] T>
     ptr<[imm] T> can be assigned ptr<[mut] T>
  */
  if (target_type->is<Type::Pointer>() && value_type->is<Type::Pointer>()) {
    const Type::Pointer& target_ptr = target_type->as<Type::Pointer>();
    const Type::Pointer& value_ptr = value_type->as<Type::Pointer>();
    bool mut_compatible = target_ptr.is_pointee_mutable ? value_ptr.is_pointee_mutable : true;
    if (target_ptr.pointee && value_ptr.pointee &&
        *(target_ptr.pointee) == *(value_ptr.pointee) &&
        mut_compatible) {
      return true;
    }
  }

  // otherwise we have found a type error on assignemnet
  m_logger->report(Diag::TypeMismatch(span, target_type->to_string(), value_type->to_string()));
  return false;
}

bool TypeChecker::expect_specific_type(const ExprPtr& expr, const Type& expected_type_val) {
  Type* actual_type = get_expr_type(expr);
  if (!actual_type) return false;

  if (actual_type->is<Type::ErrorType>()) return true;
  if (*actual_type == expected_type_val) return true;

  m_logger->report(Diag::TypeMismatch(expr->token->get_span(), expected_type_val.to_string(), actual_type->to_string()));
  return false;
}

bool TypeChecker::expect_boolean_type(const ExprPtr& expr) {
  Type* bool_type = m_symtab->lookup<Type>("bool", 0);
  _assert(bool_type, "Bool type not found in symbol table");
  return expect_specific_type(expr, *bool_type);
}

// == Poison Nodes ==
void TypeChecker::visit(PoisonExprNode& node) {
  // syntax was invalid, parser already logged it. propagate poison upward.
  node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
}

void TypeChecker::visit(PoisonStmtNode& node) {
  // syntax was invalid, do nothing further.
}

// == Literal visitors ==
void TypeChecker::visit(IntegerLiteralNode& node) {
  node.expr_type = m_symtab->lookup<Type>("i64", 0);
  _assert(node.expr_type != nullptr, "i64 type not found for IntLiteral");
}

void TypeChecker::visit(FloatLiteralNode& node) {
  node.expr_type = m_symtab->lookup<Type>("f64", 0);
  _assert(node.expr_type != nullptr, "f64 type not found for FloatLiteral");
}

void TypeChecker::visit(StringLiteralNode& node) {
  node.expr_type = m_symtab->lookup<Type>("string", 0);
  _assert(node.expr_type != nullptr, "string type not found for StringLit");
}

void TypeChecker::visit(BoolLiteralNode& node) {
  node.expr_type = m_symtab->lookup<Type>("bool", 0);
  _assert(node.expr_type != nullptr, "bool type not found for BoolLiteral");
}

void TypeChecker::visit(NullLiteralNode& node) {
  node.expr_type = m_symtab->lookup<Type>("u0", 0);
  _assert(node.expr_type != nullptr, "u0 type not found for NullLiteral");
}

void TypeChecker::visit(CharLiteralNode& node) {
  node.expr_type = m_symtab->lookup<Type>("u8", 0);
  _assert(node.expr_type != nullptr, "u8 type not found for CharLiteral");
}

// == Identifier ==
void TypeChecker::visit(IdentifierNode& node) {
  // The parser handles declaring all identifiers, so we just have to search it
  Variable* var_symbol = m_symtab->lookup<Variable>(node.name_str, node.scope_id);

  if (!var_symbol) {
    m_logger->report(Diag::VariableNotFound(node.token->get_span(), std::string(node.name_str)));
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    return;
  }

  // check for use-before-declaration (functions can do this though)
  bool is_function = var_symbol->type && var_symbol->type->is<Type::Function>();
  if (!is_function && var_symbol->span > node.token->get_span()) {
    m_logger->report(Diag::Error(node.token->get_span(), "'" + std::string(node.name_str) + "' used before its declaration."));
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    return;
  }

  if (!var_symbol->type) {
    // This can happen if type inference failed for this variable earlier.
    m_logger->report(Diag::Error(node.token->get_span(), "Type of variable '" + std::string(node.name_str) + "' could not be determined."));
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    return;
  }

  // otherwise we simply resolve using the variables declared type.
  // We should've already stored the resolved type from a var declaration node
  node.expr_type = var_symbol->type;
}

// == Grouped Expression ==
void TypeChecker::visit(GroupedExprNode& node) {
  // The type of a grouped expression is the type of its inner expression.
  node.expr_type = get_expr_type(node.expression);
}

/* returns type if it is a mutable/assignable lvalue, otherwise nullptr */
Type* TypeChecker::get_lvalue_type_if_mutable(const ExprPtr& expr) {
  _assert(expr != nullptr, "ExprPtr should not be null (poisoned at least)");

  if (auto grouped_node = dynamic_cast<GroupedExprNode*>(expr)) {
    return get_lvalue_type_if_mutable(grouped_node->expression);
  }

  if (auto ident_node = dynamic_cast<IdentifierNode*>(expr)) {
    Variable* var_sym = m_symtab->lookup<Variable>(ident_node->name_str, ident_node->scope_id);
    if (var_sym && var_sym->type &&
        (var_sym->modifier == BorrowState::MutablyOwned ||
         var_sym->modifier == BorrowState::MutablyBorrowed ||
         var_sym->is_return_var)) {
      return var_sym->type;
    }
    return nullptr;
  }

  if (auto deref_node = dynamic_cast<UnaryExprNode*>(expr)) {
    if (deref_node->op_type == UnaryOperator::Dereference) {
      // For *p = ..., p must be ptr<mut T> for it to be mutable
      // The type of p (deref_lvalue->operand) must be ptr<mut T>.
      Type* operand_type = get_expr_type(deref_node->operand);
      if (operand_type && operand_type->is<Type::Pointer>() &&
          operand_type->as<Type::Pointer>().is_pointee_mutable) {
        return operand_type->as<Type::Pointer>().pointee;
      }
    }
    return nullptr;
  }

  if (auto field_access_node = dynamic_cast<FieldAccessNode*>(expr)) {
    // For s.f or p->f, we need to check mutability of s or pointee of p.
    Type* object_type = get_expr_type(field_access_node->object);
    if (!object_type) {
      return nullptr;
    }

    if (object_type->is<Type::Pointer>()) {
      // pointer access: p.f (auto-deref). Mutable if p is ptr<mut S>.
      if (object_type->as<Type::Pointer>().is_pointee_mutable) {
        return get_expr_type(expr); // return field's type
      }
    } else {
      // struct value access: s.f is mutable if s is a mutable variable.
      if (get_lvalue_type_if_mutable(field_access_node->object)) {
        return get_expr_type(expr);
      }
    }
    return nullptr;
  }

  if (auto array_index_node = dynamic_cast<ArrayIndexNode*>(expr)) {
    // Pointers: p[i] = ..., p must be ptr<mut T>
    Type* object_type = get_expr_type(array_index_node->object);
    if (object_type && object_type->is<Type::Pointer>() &&
        object_type->as<Type::Pointer>().is_pointee_mutable) {
      return get_expr_type(expr);
    }

    // Strings: s[i] = ..., s must be mutable
    if (object_type && object_type->is<Type::Named>() &&
        object_type->as<Type::Named>().identifier == "string") {
      if (auto ident_node = dynamic_cast<IdentifierNode*>(array_index_node->object)) {
        Variable* var_sym = m_symtab->lookup<Variable>(ident_node->name_str, ident_node->scope_id);
        if (var_sym && (var_sym->modifier == BorrowState::MutablyOwned ||
                        var_sym->modifier == BorrowState::MutablyBorrowed)) {
          return get_expr_type(expr);
        }
      }
    }
    return nullptr;
  }

  return nullptr;
}

// == Statement Nodes ==
void TypeChecker::visit(AssignmentNode& node) {
  Type* lvalue_type = get_expr_type(node.lvalue);
  Type* rvalue_type = get_expr_type(node.rvalue);

  if (!lvalue_type || !rvalue_type ||
      lvalue_type->is<Type::ErrorType>() || rvalue_type->is<Type::ErrorType>()) {
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    return;
  }

  // Check to make sure the L-value is assignable:
  //   mutable var, dereference of mutable ptr, field of struct
  Type* mutable_lvalue_type = get_lvalue_type_if_mutable(node.lvalue);
  if (!mutable_lvalue_type) {
    m_logger->report(Diag::Error(node.lvalue->token->get_span(), "L-value is not assignable (not mutable or not a valid target)."));
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    return;
  }

  // Re-check type consistency, though lvalue_type should be mutable_lvalue_type
  _assert(lvalue_type && *lvalue_type == *mutable_lvalue_type, "L-value type should be mutable_lvalue_type during assignment check");

  if (check_type_assignable(lvalue_type, rvalue_type, node.rvalue->token->get_span())) {
    node.expr_type = lvalue_type;
  } else {
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
  }
}

// == Variable Declaration ==
void TypeChecker::visit(VariableDeclNode& node) {
  Type* var_declared_type = node.type;
  Type* initializer_expr_type = nullptr;

  if (node.initializer) {
    initializer_expr_type = get_expr_type(node.initializer);
    if (!initializer_expr_type) {
      node.name->expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
      return;
    }
  }

  Variable* sym = m_symtab->lookup<Variable>(node.name->name_str, node.scope_id);
  _assert(sym, "Variable should've been declared in symtab by Parser.");

  if (var_declared_type) {
    // Explicit type declaration
    if (initializer_expr_type && !initializer_expr_type->is<Type::ErrorType>()) {
      if (!check_type_assignable(var_declared_type, initializer_expr_type,
                                 node.initializer->token->get_span())) {
        // error already reported by check_type_assignable
        // we don't need to return early, declared types should still be assigned
      }
    }

    // Resolve the types
    sym->type = var_declared_type;
    node.name->expr_type = var_declared_type;
  } else {
    // Type inference
    _assert(initializer_expr_type != nullptr,
            "Parser should require initializer expression after `:=`");
    if (initializer_expr_type && !initializer_expr_type->is<Type::ErrorType>()) {
      // infer the type from the initializer and resolve with that type
      sym->type = initializer_expr_type;
      node.name->expr_type = initializer_expr_type;
    } else {
      sym->type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
      node.name->expr_type = sym->type;
    }

    // update the AST node's type field which was nullptr for `:=`
    node.type = initializer_expr_type;
  }
}

// Block Statement
void TypeChecker::visit(BlockNode& node) {
  for (const StmtPtr& stmt : node.statements) {
    stmt->accept(*this);
  }
}

// Expression Statement
void TypeChecker::visit(ExpressionStatementNode& node) {
  // resolve type of the expression
  get_expr_type(node.expression);
}

// Read statement
void TypeChecker::visit(ReadStmtNode& node) {
  Type* expr_type = get_expr_type(node.expression);
  if (!expr_type || expr_type->is<Type::ErrorType>()) return;

  if (!get_lvalue_type_if_mutable(node.expression)) {
    m_logger->report(Diag::Error(node.expression->token->get_span(), "Expression for 'read' statement must be a mutable l-value"));
    return;
  }

  bool is_allowed_type = is_integer_type(expr_type) ||
                         (expr_type->is<Type::Named>() &&
                          expr_type->as<Type::Named>().identifier == "string");

  if (!is_allowed_type) {
    m_logger->report(Diag::Error(
        node.expression->token->get_span(),
        "Cannot 'read' into variable of type '" + expr_type->to_string() +
            "'. Must be a mutable integer or string variable."));
  }
}

// Print Statement
void TypeChecker::visit(PrintStmtNode& node) {
  for (size_t i = 0; i < node.expressions.size(); ++i) {
    Type* expr_type = get_expr_type(node.expressions[i]);
    if (!expr_type) continue;

    // Anything except u0 can be printed for now
    if (!expr_type->is<Type::ErrorType>() &&
        expr_type->is<Type::Named>() &&
        expr_type->as<Type::Named>().identifier == "u0") {
      m_logger->report(Diag::Warning(node.expressions[i]->token->get_span(), "Printing a 'u0' (void) value is not allowed."));
    }
  }
}

// If Statement
void TypeChecker::visit(IfStmtNode& node) {
  if (!expect_boolean_type(node.condition)) {
    // Error in condition, but continue to resolve the branches
  }
  node.then_branch->accept(*this);
  if (node.else_branch) {
    node.else_branch->accept(*this);
  }
}

// While Statement
void TypeChecker::visit(WhileStmtNode& node) {
  if (!expect_boolean_type(node.condition)) {
    // Error in condition, but continue to resolve the body
  }
  bool prev_in_loop = m_in_loop;
  m_in_loop = true;
  node.body->accept(*this);
  m_in_loop = prev_in_loop;
}

// For Statement
void TypeChecker::visit(ForStmtNode& node) {
  // For statement structure: for (initializer; condition; iteration) body
  if (node.initializer.has_value()) {
    std::visit(
        [this](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, ExprPtr>) {
            if (arg) get_expr_type(arg);
          } else if constexpr (std::is_same_v<T, StmtPtr>) {
            if (arg) arg->accept(*this);
          }
        },
        *node.initializer);
  }

  // condition defaults to true
  if (node.condition) expect_boolean_type(node.condition);
  if (node.iteration) get_expr_type(node.iteration);

  bool prev_in_loop = m_in_loop;
  m_in_loop = true;
  node.body->accept(*this);
  m_in_loop = prev_in_loop;
}

// Break and Continue Statements
void TypeChecker::visit(BreakStmtNode& node) {
  if (!m_in_loop) {
    m_logger->report(Diag::Error(node.token->get_span(), "'break' statement not within a loop."));
  }
}

void TypeChecker::visit(ContinueStmtNode& node) {
  if (!m_in_loop) {
    m_logger->report(Diag::Error(node.token->get_span(), "'continue' statement not within a loop."));
  }
}

// Return Statement
void TypeChecker::visit(ReturnStmtNode& node) {
  if (!m_current_function_return_type) {
    m_logger->report(Diag::Error(node.token->get_span(), "'return' statement outside of a function context."));
    return;
  }

  Type* u0_type = m_symtab->lookup<Type>("u0", 0);
  _assert(u0_type, "u0 type not found in symbol table");

  if (node.value) {
    // Return with a value: return <expr>;
    Type* actual_return_type = get_expr_type(node.value);
    if (!actual_return_type || actual_return_type->is<Type::ErrorType>()) return;
    if (*m_current_function_return_type == *u0_type) {
      m_logger->report(Diag::Error(node.value->token->get_span(), "Function declared to return 'u0' cannot return a value."));
      return;
    }
    // make sure the type returned is correct
    check_type_assignable(m_current_function_return_type, actual_return_type, node.value->token->get_span());
  } else {
    // Return without a value must be a part of a void function, else err
    if (!(*m_current_function_return_type == *u0_type)) {
      m_logger->report(Diag::Error(node.token->get_span(), "Non-u0 function must return a value. Expected'" + m_current_function_return_type->to_string() + "'."));
    }
  }
}

// Function Declaration
void TypeChecker::visit(FunctionDeclNode& node) {
  Type* prev_return_type = m_current_function_return_type;
  m_current_function_return_type = node.return_type;
  _assert(m_current_function_return_type, "Parser should set func return_t");

  // Parameters are declared by the parser. Their types are not optional.
  std::vector<Type::ParamInfo> param_infos;
  for (ParamPtr param : node.params) {
    param->accept(*this);
    param_infos.push_back({param->type, param->modifier});
  }

  // Type check the function body
  node.body->accept(*this);

  // TODO: perform path analysis for return stmts

  /* confirm that this function exists in the symbol table */
  size_t scope = node.scope_id;
  Type fn_type(Type::Function(std::move(param_infos), node.return_type), scope);
  Type* t = m_symtab->lookup<Type>(fn_type.to_string(), scope);
  _assert(t != nullptr, "Parser should declare the func type");

  // restore context:
  m_current_function_return_type = prev_return_type;
}

// ParamNode
void TypeChecker::visit(ParamNode& node) {
  _assert(node.type != nullptr,
          "Parser should enforce that params are explicitly typed");
  node.name->expr_type = node.type; // just resolve the identifier expr within
}

// Struct Declaration
void TypeChecker::visit(StructDeclNode& node) {
  // resolve fields
  // set the size of the struct based on its fields.
  std::uint64_t current_struct_size = 0;
  for (const StructFieldPtr& field : node.fields) {
    field->accept(*this);
    _assert(field->type, "field type should be non-null");
    if (field->type) current_struct_size += field->type->get_byte_size();
  }
  node.struct_size = current_struct_size;
}

// Struct Field Node
void TypeChecker::visit(StructFieldNode& node) {
  _assert(node.type != nullptr,
          "Parser should enforce that struct fields must be explicitly typed");
  node.name->expr_type = node.type; // just resolve the identifier expr within
}

// == Binary Operations ==
void TypeChecker::visit(BinaryOpExprNode& node) {
  Type* left_type = get_expr_type(node.left);
  Type* right_type = get_expr_type(node.right);

  if (!left_type || !right_type || left_type->is<Type::ErrorType>() || right_type->is<Type::ErrorType>()) {
      node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
      return;
  }

  Type* result_type = nullptr;
  Type* bool_type = m_symtab->lookup<Type>("bool", 0);
  Type* i64_type = m_symtab->lookup<Type>("i64", 0);
  Type* f64_type = m_symtab->lookup<Type>("f64", 0);
  Type* string_type = m_symtab->lookup<Type>("string", 0);
  Type* null_type = m_symtab->lookup<Type>("u0", 0);

  switch (node.op_type) {
    case BinOperator::Plus:
      if (is_integer_type(left_type) && is_integer_type(right_type))
        result_type = i64_type;
      else if (is_numeric_type(left_type) && is_numeric_type(right_type))
        result_type = f64_type;
      else if ((*left_type == *string_type && *right_type == *string_type))
        result_type = string_type;

      // Pointer arithmetic: ptr<T> + i64 -> ptr<T> or i64 + ptr<T> -> ptr<T>
      else if (left_type->is<Type::Pointer>() && is_integer_type(right_type))
        result_type = left_type;
      else if (is_integer_type(left_type) && right_type->is<Type::Pointer>())
        result_type = right_type;
      break;

    case BinOperator::Minus:
      if (is_integer_type(left_type) && is_integer_type(right_type))
        result_type = i64_type;
      else if (is_numeric_type(left_type) && is_numeric_type(right_type))
        result_type = f64_type;

      // Pointer subtraction: ptr<T> - ptr<T> -> i64
      else if (left_type->is<Type::Pointer>() &&
               right_type->is<Type::Pointer>() && *left_type == *right_type)
        result_type = i64_type;
      // Pointer subtraction: ptr<T> - i64 -> ptr<T>
      else if (left_type->is<Type::Pointer>() && is_integer_type(right_type))
        result_type = left_type;
      break;

    case BinOperator::Multiply:
    case BinOperator::Divide:
      if (is_integer_type(left_type) && is_integer_type(right_type))
        result_type = i64_type;
      else if (is_numeric_type(left_type) && is_numeric_type(right_type))
        result_type = f64_type;
      break;
    case BinOperator::Modulo:
      if (is_integer_type(left_type) && is_integer_type(right_type))
        result_type = i64_type;
      break;

    case BinOperator::Equal:
    case BinOperator::NotEqual:
      // General equality: types must be the same, or one is ptr and other is
      // null
      if (*left_type == *right_type ||
          (is_numeric_type(left_type) && is_numeric_type(right_type)))
        result_type = bool_type;
      else if (left_type->is<Type::Pointer>() && *right_type == *null_type)
        result_type = bool_type;
      else if (*left_type == *null_type && right_type->is<Type::Pointer>())
        result_type = bool_type;
      break;

    case BinOperator::LessThan:
    case BinOperator::LessEqual:
    case BinOperator::GreaterThan:
    case BinOperator::GreaterEqual:
      // Ordered comparison: types must be same numeric or pointer types
      // (excluding null for direct < >)
      if ((is_numeric_type(left_type) && is_numeric_type(right_type)) ||
          (left_type->is<Type::Pointer>() && right_type->is<Type::Pointer>() &&
           *left_type == *right_type)) {
        result_type = bool_type;
      }
      break;

    case BinOperator::LogicalAnd:
    case BinOperator::LogicalOr:
      if ((*left_type == *bool_type && *right_type == *bool_type))
        result_type = bool_type;
      break;
    default:
      m_logger->report(Diag::Fatal(node.token->get_span(),
                             "Internal Error: Unsupported binary operator '" +
                                 std::string(node.token->get_lexeme()) + "'."));
      break;
  }

  if (!result_type) {
    m_logger->report(Diag::TypeMismatch(node.token->get_span(), "compatible types",
                           left_type->to_string() + " and " +
                           right_type->to_string() + ". binary operator '" +
                           std::string(node.token->get_lexeme()) + "'"));
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
  } else {
    node.expr_type = result_type;
  }
}

// Unary Operation
void TypeChecker::visit(UnaryExprNode& node) {
  Type* operand_type = get_expr_type(node.operand);
  if (!operand_type || operand_type->is<Type::ErrorType>()) {
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    return;
  }

  Type* result_type = nullptr;
  Type* bool_type = m_symtab->lookup<Type>("bool", 0);
  Type* i64_type = m_symtab->lookup<Type>("i64", 0);
  Type* f64_type = m_symtab->lookup<Type>("f64", 0);

  switch (node.op_type) {
    case UnaryOperator::Negate: // '-'
      if (is_numeric_type(operand_type)) {
        result_type = operand_type;
      } else {
        m_logger->report(Diag::TypeMismatch(node.token->get_span(), "numeric type", operand_type->to_string() + ". unary negation '-'"));
      }
      break;
    case UnaryOperator::LogicalNot: // '!'
      if (*operand_type == *bool_type) {
        result_type = bool_type;
      } else {
        m_logger->report(Diag::TypeMismatch(node.token->get_span(), "boolean", operand_type->to_string() + ". logical not '!'"));
      }
      break;
    case UnaryOperator::AddressOf:    // '&expr' -> ptr<imm typeof(expr)>
    case UnaryOperator::AddressOfMut: // '&mut expr' -> ptr<mut typeof(expr)>
    {
      bool is_mut = (node.op_type == UnaryOperator::AddressOfMut);

      if (!is_mut || get_lvalue_type_if_mutable(node.operand)) {
        Type storage(Type::Pointer(is_mut, operand_type), node.scope_id);
        std::string_view type_name = m_arena->make_string(storage.to_string());

        result_type = m_symtab->lookup<Type>(type_name, node.scope_id);
        if (!result_type) {
          result_type = m_symtab->declare_type(type_name, m_arena->make<Type>(storage), node.scope_id);
        }
        _assert(result_type,
                (is_mut ? "Failed to get/declare pointer type for AddressOfMut"
                        : "Failed to get/declare pointer type for AddressOf"));
      } else {
        m_logger->report(Diag::Error(node.operand->token->get_span(), "Cannot take a mutable reference to a non-mutable l-value or r-value."));
      }
    } break;
    case UnaryOperator::Dereference: // '*expr'
      if (operand_type->is<Type::Pointer>()) {
        result_type = operand_type->as<Type::Pointer>().pointee;
        if (!result_type) { // Should not happen if pointer types are always valid
          m_logger->report(Diag::Error(node.token->get_span(), "Cannot dereference pointer to an unresolved or void type."));
        }
      } else {
        m_logger->report(Diag::TypeMismatch(node.token->get_span(), "pointer type", operand_type->to_string() + ". dereference '*'"));
      }
      break;
    default:
      m_logger->report(Diag::Fatal(node.token->get_span(), "Internal Error: Unsupported unary operator."));
      break;
  }
  if (!result_type) {
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
  } else {
    node.expr_type = result_type;
  }
}

void TypeChecker::visit(FunctionCallNode& node) {
  Type* callee_type = get_expr_type(node.callee);

  if (!callee_type || callee_type->is<Type::ErrorType>()) {
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    return;
  }

  if (!callee_type->is<Type::Function>()) {
    m_logger->report(Diag::Error(node.callee->token->get_span(), "'" + std::string(node.callee->token->get_lexeme()) + "' is not callable as a function."));
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    return;
  }

  const Type::Function& func_type_data = callee_type->as<Type::Function>();

  if (node.arguments.size() != func_type_data.params.size()) {
    m_logger->report(
        Diag::Error(node.callee->token->get_span(),
              "Function '" + std::string(node.callee->token->get_lexeme()) +
                  "' called with incorrect number of arguments. Expected " +
                  std::to_string(func_type_data.params.size()) + ", got " +
                  std::to_string(node.arguments.size()) + "."));
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    return;
  }

  for (size_t i = 0; i < node.arguments.size(); ++i) {
    // ensure argument expression itself is typed
    node.arguments[i]->accept(*this);

    ArgPtr arg = node.arguments[i];
    Type* arg_t = arg->expression->expr_type;

    if (!arg_t || arg_t->is<Type::ErrorType>()) {
      node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
      return;
    }

    const Type::ParamInfo& param_info = func_type_data.params[i];
    Type* param_type = param_info.type;

    _assert(
        param_type != nullptr,
        "Param should be resolved with explicit type before a function call "
        "can occur for it");

    const Span& span = node.arguments[i]->expression->token->get_span();
    if (!check_type_assignable(param_type, arg_t, span)) {
      node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
      return;
    }

    if (param_info.modifier == BorrowState::MutablyBorrowed &&
        !get_lvalue_type_if_mutable(node.arguments[i]->expression)) {
      m_logger->report(Diag::Error(span, "Argument for 'mut' parameter must be a mutable l-value."));
      node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
      return;
    }

    if (arg->is_give) {
      // the parameter must be either mutably owned or imm owned (take keyword)
      if (param_info.modifier != BorrowState::MutablyOwned &&
          param_info.modifier != BorrowState::ImmutablyOwned) {
        m_logger->report(Diag::Error(span, "Argument passed with 'give' must be associated with a parameter that has the 'take' keyword"));
        node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
        return;
      }
    }
  }

  node.expr_type = func_type_data.return_type;
}

// Other:
void TypeChecker::visit(FieldAccessNode& node) {
  Type* object_type = get_expr_type(node.object);

  if (!object_type || object_type->is<Type::ErrorType>()) {
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    return;
  }

  Type* base_object_type = object_type;
  // bool is_obj_ptr_to_mut = false;

  // Auto-dereference pointers
  if (object_type->is<Type::Pointer>()) {
    base_object_type = object_type->as<Type::Pointer>().pointee;
    // is_obj_ptr_to_mut = object_type->as<Type::Pointer>().is_pointee_mutable;
    if (!base_object_type) {
      m_logger->report(Diag::Error(node.object->token->get_span(), "Cannot access field of a pointer to an unresolved or void type."));
      node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
      return;
    }
  }

  if (!base_object_type->is<Type::Named>()) {
    m_logger->report(Diag::Error(
        node.object->token->get_span(),
        "Left side of '.' must be a struct or a pointer to a struct. Got: " +
            base_object_type->to_string()));
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    return;
  }

  const std::string_view& name = base_object_type->as<Type::Named>().identifier;
  StructDeclPtr struct_decl = m_symtab->lookup_struct(name, base_object_type->get_scope_id());
  _assert(struct_decl != nullptr, "Struct type should exist in symbol table");

  // Search for the field or method in the struct's fields
  for (const StructFieldPtr& field : struct_decl->fields) {
    if (field->name->name_str == node.field->name_str) {
      node.expr_type = field->type;
      return;
    }
  }

  m_logger->report(Diag::Error(node.field->token->get_span(), "Struct '" + std::string(name) + "' has no field named '" + std::string(node.field->name_str) + "'."));
  node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
}

void TypeChecker::visit(ArrayIndexNode& node) {
  Type* object_type = get_expr_type(node.object);
  Type* index_type = get_expr_type(node.index);

  if (!object_type || !index_type || object_type->is<Type::ErrorType>() || index_type->is<Type::ErrorType>()) {
      node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
      return;
  }

  // if the object is a string, subscripting returns the char value: u8
  if (object_type->is<Type::Named>() &&
      object_type->as<Type::Named>().identifier == "string") {
    node.expr_type = m_symtab->lookup<Type>("u8", 0);
    return;
  }

  // Pointers are used as arrays right now, we do not have Array types.
  if (!object_type->is<Type::Pointer>()) {
    m_logger->report(Diag::TypeMismatch(node.object->token->get_span(),
                                       "pointer type (for array access)",
                                       object_type->to_string()));
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    return;
  }

  if (!is_integer_type(index_type)) {
    m_logger->report(Diag::TypeMismatch(node.index->token->get_span(),
                                       "integer type (for array index)",
                                       index_type->to_string() + " found"));
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    return;
  }

  node.expr_type = object_type->as<Type::Pointer>().pointee;
}

void TypeChecker::visit(StructLiteralNode& node) {
  Type* struct_type = node.struct_decl->type;
  node.expr_type = struct_type;

  _assert(struct_type->is<Type::Named>(), "Struct literal type should be Named");

  const std::string_view& name = struct_type->as<Type::Named>().identifier;
  StructDeclPtr struct_decl = node.struct_decl;

  // Type check field initializers
  bool error_occured = false;
  std::unordered_set<std::string_view> initialized_fields;
  for (const StructFieldInitPtr& initializer : node.initializers) {
    // we do not resolve the field because it simply must be an identifier
    // that matches with the field field name
    Type* value_type = get_expr_type(initializer->value);
    if (!value_type || value_type->is<Type::ErrorType>()) {
      error_occured = true;
      continue;
    }

    if (initialized_fields.count(initializer->field->name_str)) {
      m_logger->report(Diag::Error(initializer->field->token->get_span(),
          "Duplicate initialization of field '" + std::string(initializer->field->name_str) + "'."));
      error_occured = true;
    }
    initialized_fields.insert(initializer->field->name_str);

    auto itr = std::find_if(struct_decl->fields.begin(), struct_decl->fields.end(),
                     [&](const StructFieldPtr& field) { return field->name->name_str == initializer->field->name_str; });

    if (itr == struct_decl->fields.end()) {
      error_occured = true;
      m_logger->report(Diag::Error(initializer->field->token->get_span(),
                             "Struct '" + std::string(name) + "' has no field named '" +
                                  std::string(initializer->field->name_str) + "'."));
      continue;
    }

    initializer->field->expr_type = (*itr)->type;

    if (!check_type_assignable((*itr)->type, value_type, initializer->value->token->get_span())) {
      error_occured = true;
    }
  }
  // We currently allow for partial initialization of the named fields
  if (error_occured) {
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
  }
}

void TypeChecker::visit(NewExprNode& node) {
  _assert(node.type_to_allocate != nullptr, "Parser must provide type_to_allocate");

  /*
     Result is `ptr<[mut] type_to_allocate>`
     - where [mut] is based on is_memory_mutable

     If the allocation type is an array, the specifier is size (integer)
     Else it must be a constructor, where specifier is initializer args

     Right now, if it is a constructor, it can be any of the named types:
      - Primitive
        - Check its resolved type is valid
      - Struct
        - The specifier should be of type StructLiteralNode, because
     StructLiteralNode is a valid expression, in which we can just call
     ->accept(*this) and it will be checked
  */

  // The result of `new <T>` is `ptr<imm T>`
  // The result of `new <mut T>` is `ptr<mut T>`
  // The result of `new <T>[size]` is array of the above case

  bool is_prim_alloc = is_primitive_type(node.type_to_allocate);
  bool is_valid_alloc_type = is_prim_alloc ||
                             node.type_to_allocate->is<Type::Named>() ||
                             node.type_to_allocate->is<Type::Pointer>();
  if (!is_valid_alloc_type) {
    m_logger->report(Diag::Error(node.token->get_span(), "Allocation type must be a primitive, struct, or pointer."));
    node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    return;
  }

  // Find the pointer type in the symbol table (though it may not exist
  // because the parser doesn't declare types that are used/created by new)
  Type ptr_t(Type::Pointer(node.is_memory_mutable, node.type_to_allocate), node.scope_id);
  std::string_view name = m_arena->make_string(ptr_t.to_string());

  Type* ptr_type = m_symtab->lookup<Type>(name, ptr_t.get_scope_id());
  if (!ptr_type) ptr_type = m_symtab->declare_type(name, m_arena->make<Type>(ptr_t), ptr_t.get_scope_id());
  _assert(ptr_type, "ptr type should be non-null after lookup/declaration");

  node.expr_type = ptr_type;

  if (node.allocation_specifier) {
    // this should handle if it is a struct literal expression, or regular
    // expression, due to dynamic binding/dispatch
    Type* spec_type = get_expr_type(node.allocation_specifier);
    if (!spec_type || spec_type->is<Type::ErrorType>()) {
      node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
      return;
    }

    if (node.is_array_allocation) {
      // new <T>[size], size must be an integer type

      if (!is_integer_type(spec_type)) {
        m_logger->report(Diag::TypeMismatch(
            node.allocation_specifier->token->get_span(), "i64",
            spec_type->to_string() + ". array size in new expression"));
        node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
      }
    } else {
      // new <T>(constructor_arg)
      // just have to make sure that they are assignable
      if (!check_type_assignable(node.type_to_allocate, spec_type,
                            node.allocation_specifier->token->get_span())) {
        node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
      }
    }
  } else {
    // No allocation specifier, new <T>() or new <T>[]
    if (node.is_array_allocation) {
      m_logger->report(Diag::Error(node.token->get_span(),
                "Array allocation 'new <T>[]' must have a size expression."));
      node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    }

    // else it is the default () constructor, which is only valid for structs
    else if (is_prim_alloc) {
      m_logger->report(Diag::Error(node.token->get_span(),
                    "Primitive type '" + node.type_to_allocate->to_string() +
                    "' cannot be default constructed with 'new()'. Provide "
                    "an initializer like 'new<" +
                    node.type_to_allocate->to_string() + ">(value)'."));
      node.expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    }
  }
}

void TypeChecker::visit(SwitchStmtNode& node) {
  // resolve the expression itself and all the cases
  Type* switch_expr_type = get_expr_type(node.expression);

  if (!switch_expr_type || switch_expr_type->is<Type::ErrorType>()) {
    switch_expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
  } else {
    // Check if switch expression type is comparable.
    // Allow types that have an operator== override
    if (switch_expr_type->is<Type::Function>() ||
        (switch_expr_type->is<Type::Named>() &&
         switch_expr_type->as<Type::Named>().identifier == "f64")) {
      // Disallow switching on functions or floats for now due to complexity
      m_logger->report(Diag::Error(node.expression->token->get_span(),
                    "Switch expression cannot be of type '" +
                    switch_expr_type->to_string() +
                    "'. Only equatable types like integers, strings, "
                    "pointers, bools allowed."));
      switch_expr_type = m_arena->make<Type>(Type::ErrorType{}, node.scope_id);
    }
  }

  Type* prev_switch_expr_type = m_current_switch_expr_type;
  m_current_switch_expr_type = switch_expr_type;

  bool has_default = false;
  for (const CasePtr& case_node : node.cases) {
    case_node->accept(*this);
    if (!case_node->value) {
      // default
      if (has_default) {
        m_logger->report(Diag::Error(case_node->token->get_span(),
                               "Multiple default cases in switch statement."));
      }
      has_default = true;
    }
  }

  // restore context
  m_current_switch_expr_type = prev_switch_expr_type;
}

void TypeChecker::visit(CaseNode& node) {
  if (node.value) {
    // not the 'default' case
    Type* case_value_type = get_expr_type(node.value);
    if (m_current_switch_expr_type && !m_current_switch_expr_type->is<Type::ErrorType>()) {
      if (case_value_type && !case_value_type->is<Type::ErrorType>()) {
        check_type_assignable(m_current_switch_expr_type, case_value_type,
                              node.value->token->get_span());
      }
    }
  }
  // check the contents of the case body
  _assert(node.body, "case body should be non-null");
  node.body->accept(*this);
}

void TypeChecker::visit(FreeStmtNode& node) {
  Type* expr_type = get_expr_type(node.expression);
  if (!expr_type || expr_type->is<Type::ErrorType>()) return;

  if (expr_type->is<Type::Pointer>()) {
    if (!expr_type->as<Type::Pointer>().is_pointee_mutable) {
      m_logger->report(Diag::Error(node.expression->token->get_span(),
            "Cannot 'free' an immutable pointer. Pointer must be 'ptr<mut T>'."));
      return;
    }
  }

  bool is_string = expr_type->is<Type::Named>() &&
                   expr_type->as<Type::Named>().identifier == "string";
  bool is_ptr = expr_type->is<Type::Pointer>();

  bool is_struct = false;
  if (expr_type->is<Type::Named>()) {
    StructDeclPtr struct_decl = m_symtab->lookup_struct(expr_type->as<Type::Named>().identifier, expr_type->get_scope_id());
    if (struct_decl != nullptr) is_struct = true;
  }

  if (!is_ptr && !is_string && !is_struct) {
    m_logger->report(Diag::TypeMismatch(
        node.expression->token->get_span(), "pointer, string, or struct type",
        expr_type->to_string() + ". 'free' statement operand"));
  }
  // array vs not array deallocation will be up to the user.
}

void TypeChecker::visit(ExitStmtNode& node) {
  if (node.exit_code < 0) {
    m_logger->report(Diag::Error(node.token->get_span(), "Exit code must be a non-negative integer."));
  }
}

// No type checking needed for these two
void TypeChecker::visit(ErrorStmtNode&) {}
void TypeChecker::visit(AsmBlockNode&) {}
void TypeChecker::visit(StructFieldInitializerNode& node) {
  // just resolve the value it has, and then caller from StructLiteralNode
  // should handle making sure it is correct based on the struct declaration.
  get_expr_type(node.value);
}
void TypeChecker::visit(ArgumentNode& node) {
  // 'give' argument is handled from within function call node
  get_expr_type(node.expression);
}
