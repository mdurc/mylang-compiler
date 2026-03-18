#ifndef PARSER_AST_H
#define PARSER_AST_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../lexer/token.h"
#include "types.h"

/* TODO: get rid of these renames? */
using AstPtr = class AstNode*;
using ExprPtr = class ExpressionNode*;
using StmtPtr = class StatementNode*;
using IdentPtr = class IdentifierNode*;
using BlockPtr = class BlockNode*;

using ArgPtr = class ArgumentNode*;
using StructFieldInitPtr = class StructFieldInitializerNode*;
using CasePtr = class CaseNode*;
using StructFieldPtr = class StructFieldNode*;
using ParamPtr = class ParamNode*;
using FuncDeclPtr = class FunctionDeclNode*;
using StructDeclPtr = class StructDeclNode*;

class Visitor;

enum BinOperator {
  Plus,     GreaterThan,
  Minus,    GreaterEqual,
  Divide,   LessThan,
  Modulo,   LessEqual,
  Multiply, LogicalAnd,
  Equal,    LogicalOr,
  NotEqual,
};

enum UnaryOperator { Negate, LogicalNot, Dereference, AddressOf, AddressOfMut };

#define DERIVE_NODE(Name, Base)       \
  class Name : public Base {          \
    using Base::Base;                 \
    void accept(Visitor& v) override; \
  };

#define DERIVE_ABSTRACT_NODE(Name, Base) \
  class Name : public Base {             \
    using Base::Base;                    \
  };

#define LITERAL_NODE(Name, T)                \
  class Name : public ExpressionNode {       \
  public:                                    \
    T value;                                 \
    Name(const Token* tok, size_t sc, T val) \
        : ExpressionNode(tok, sc),           \
          value(std::move(val)) {}           \
    void accept(Visitor& v) override;        \
  };

class AstNode {
public:
  const Token* token; /* for location and error handling */
  size_t scope_id;    /* id from symbol table */

  AstNode(const Token* tok, size_t sc) : token(tok), scope_id(sc) {}
  virtual ~AstNode() = default;
  virtual void accept(Visitor& v) = 0;
};

// == Base Abstract Nodes (Expressions and Statements) ==
class ExpressionNode : public AstNode {
public:
  Type* expr_type = nullptr; /* filled by type checker */
  using AstNode::AstNode;
};
DERIVE_ABSTRACT_NODE(StatementNode, AstNode)

// == Literal Nodes ==
LITERAL_NODE(IntegerLiteralNode, std::uint64_t)
LITERAL_NODE(FloatLiteralNode, double)
LITERAL_NODE(StringLiteralNode, std::string)
LITERAL_NODE(CharLiteralNode, std::uint64_t)
LITERAL_NODE(BoolLiteralNode, bool)
DERIVE_NODE(NullLiteralNode, ExpressionNode)

// == Basic Nodes ==
DERIVE_NODE(BreakStmtNode, StatementNode)
DERIVE_NODE(ContinueStmtNode, StatementNode)

// == Error handling Nodes ==
class PoisonExprNode : public ExpressionNode {
public:
  PoisonExprNode(const Token* tok, size_t sc) : ExpressionNode(tok, sc) {}
  void accept(Visitor& v) override;
};
class PoisonStmtNode : public StatementNode {
public:
  PoisonStmtNode(const Token* tok, size_t sc) : StatementNode(tok, sc) {}
  void accept(Visitor& v) override;
};

// == Expression Nodes ==
class IdentifierNode : public ExpressionNode {
public:
  std::string_view name_str;
  IdentifierNode(const Token* tok, size_t sc, std::string_view name_str)
      : ExpressionNode(tok, sc), name_str(name_str) {}
  void accept(Visitor& v) override;
};

class AssignmentNode : public ExpressionNode {
public:
  ExprPtr lvalue;
  ExprPtr rvalue;
  AssignmentNode(const Token* tok, size_t sc, ExprPtr lval, ExprPtr rval)
      : ExpressionNode(tok, sc), lvalue(lval), rvalue(rval) {}
  void accept(Visitor& v) override;
};

class ImplicitCastNode : public ExpressionNode {
public:
  ExprPtr expression;
  Type* target_type;
  ImplicitCastNode(const Token* tok, size_t sc, ExprPtr expr, Type* target)
      : ExpressionNode(tok, sc), expression(expr), target_type(target) {
      this->expr_type = target;
  }
  void accept(Visitor& v) override;
};

class BinaryOpExprNode : public ExpressionNode {
public:
  BinOperator op_type;
  ExprPtr left;
  ExprPtr right;
  BinaryOpExprNode(const Token* tok, size_t sc, BinOperator op, ExprPtr l, ExprPtr r)
      : ExpressionNode(tok, sc), op_type(op), left(l), right(r) {}
  void accept(Visitor& v) override;
};

class UnaryExprNode : public ExpressionNode {
public:
  UnaryOperator op_type;
  ExprPtr operand;
  UnaryExprNode(const Token* tok, size_t sc, UnaryOperator op, ExprPtr expr)
      : ExpressionNode(tok, sc), op_type(op), operand(expr) {}
  void accept(Visitor& v) override;
};

class ArgumentNode : public AstNode {
public:
  bool is_give; /* for 'give' keyword */
  ExprPtr expression;
  ArgumentNode(const Token* tok, size_t sc, bool give, ExprPtr expr)
      : AstNode(tok, sc), is_give(give), expression(expr) {}
  void accept(Visitor& v) override;
};

class FunctionCallNode : public ExpressionNode {
public:
  ExprPtr callee;
  std::vector<ArgPtr> arguments;
  FunctionCallNode(const Token* tok, size_t sc, ExprPtr fn_callee, std::vector<ArgPtr> args)
      : ExpressionNode(tok, sc), callee(fn_callee), arguments(std::move(args)) {}
  void accept(Visitor& v) override;
};

class FieldAccessNode : public ExpressionNode {
public:
  ExprPtr object;
  IdentPtr field; /* syntax: <object>.<field> */
  FieldAccessNode(const Token* tok, size_t sc, ExprPtr obj_expr, IdentPtr field)
      : ExpressionNode(tok, sc), object(obj_expr), field(field) {}
  void accept(Visitor& v) override;
};

class ArrayIndexNode : public ExpressionNode {
public:
  ExprPtr object;
  ExprPtr index; /* syntax: <object>[<index>] */
  ArrayIndexNode(const Token* tok, size_t sc, ExprPtr arr_expr, ExprPtr idx_expr)
      : ExpressionNode(tok, sc), object(arr_expr), index(idx_expr) {}
  void accept(Visitor& v) override;
};

class GroupedExprNode : public ExpressionNode {
public:
  ExprPtr expression;
  GroupedExprNode(const Token* tok, size_t sc, ExprPtr expr)
      : ExpressionNode(tok, sc), expression(expr) {}
  void accept(Visitor& v) override;
};

class StructFieldInitializerNode : public AstNode {
public:
  IdentPtr field;
  ExprPtr value;
  StructFieldInitializerNode(const Token* tok, size_t sc, IdentPtr field, ExprPtr val)
      : AstNode(tok, sc), field(field), value(val) {}
  void accept(Visitor& v) override;
};

class StructLiteralNode : public ExpressionNode {
public:
  StructDeclPtr struct_decl;
  std::vector<StructFieldInitPtr> initializers;
  StructLiteralNode(const Token* tok, size_t sc, StructDeclPtr struct_decl, std::vector<StructFieldInitPtr> inits)
      : ExpressionNode(tok, sc), struct_decl(struct_decl), initializers(std::move(inits)) {}
  void accept(Visitor& v) override;
};

class NewExprNode : public ExpressionNode {
public:
  bool is_memory_mutable;
  bool is_array_allocation;
  Type* type_to_allocate;
  ExprPtr allocation_specifier;
  NewExprNode(const Token* tok, size_t sc, bool is_mut, bool is_array, Type* allocated_type, ExprPtr specifier)
      : ExpressionNode(tok, sc), is_memory_mutable(is_mut), is_array_allocation(is_array),
        type_to_allocate(allocated_type), allocation_specifier(specifier) {}
  void accept(Visitor& v) override;
};

// == Statement Nodes ==
class VariableDeclNode : public StatementNode {
public:
  bool is_mutable;
  IdentPtr name;
  Type* type;          /* optional for ':=' type inference */
  ExprPtr initializer; /* optional if not an initialization stmt */
  VariableDeclNode(const Token* tok, size_t sc, bool mut, IdentPtr name, Type* tk, ExprPtr init)
      : StatementNode(tok, sc), is_mutable(mut), name(name),
        type(tk), initializer(init) {}
  void accept(Visitor& v) override;
};

class BlockNode : public StatementNode {
public:
  std::vector<StmtPtr> statements;
  BlockNode(const Token* tok, size_t sc, std::vector<StmtPtr> stmts)
      : StatementNode(tok, sc), statements(std::move(stmts)) {}
  void accept(Visitor& v) override;
};

class IfStmtNode : public StatementNode {
public:
  ExprPtr condition;
  BlockPtr then_branch;
  BlockPtr else_branch; /* optional */
  IfStmtNode(const Token* tok, size_t sc, ExprPtr cond, BlockPtr then_b, BlockPtr else_b)
      : StatementNode(tok, sc), condition(cond), then_branch(then_b), else_branch(else_b) {}
  void accept(Visitor& v) override;
};

class ForStmtNode : public StatementNode {
public:
  std::optional<std::variant<ExprPtr, StmtPtr>> initializer; /* has-value -> non-nullptr */
  ExprPtr condition; /* optional */
  ExprPtr iteration; /* optional */
  BlockPtr body;
  ForStmtNode(const Token* tok, size_t sc,
              std::optional<std::variant<ExprPtr, StmtPtr>> init, ExprPtr cond,
              ExprPtr iter, BlockPtr b)
      : StatementNode(tok, sc), initializer(std::move(init)), condition(cond),
        iteration(iter), body(b) {}
  void accept(Visitor& v) override;
};

class WhileStmtNode : public StatementNode {
public:
  ExprPtr condition;
  BlockPtr body;
  WhileStmtNode(const Token* tok, size_t sc, ExprPtr cond, BlockPtr b)
      : StatementNode(tok, sc), condition(cond), body(b) {}
  void accept(Visitor& v) override;
};

class CaseNode : public AstNode {
public:
  ExprPtr value; /* nullptr for default case */
  BlockPtr body;
  CaseNode(const Token* tok, size_t sc, BlockPtr b, ExprPtr val)
      : AstNode(tok, sc), value(val), body(b) {}
  void accept(Visitor& v) override;
};

class SwitchStmtNode : public StatementNode {
public:
  ExprPtr expression;
  std::vector<CasePtr> cases;
  SwitchStmtNode(const Token* tok, size_t sc, ExprPtr expr,
                 std::vector<CasePtr> c)
      : StatementNode(tok, sc), expression(expr), cases(std::move(c)) {}
  void accept(Visitor& v) override;
};

class ReadStmtNode : public StatementNode {
public:
  ExprPtr expression; /* where to store memory we read */
  ReadStmtNode(const Token* tok, size_t sc, ExprPtr expr)
      : StatementNode(tok, sc), expression(expr) {}
  void accept(Visitor& v) override;
};

class PrintStmtNode : public StatementNode {
public:
  std::vector<ExprPtr> expressions;
  PrintStmtNode(const Token* tok, size_t sc, std::vector<ExprPtr> expr)
      : StatementNode(tok, sc), expressions(std::move(expr)) {}
  void accept(Visitor& v) override;
};

class ExpressionStatementNode : public StatementNode {
public:
  ExprPtr expression;
  ExpressionStatementNode(const Token* tok, size_t sc, ExprPtr expr)
      : StatementNode(tok, sc), expression(expr) {}
  void accept(Visitor& v) override;
};

class ReturnStmtNode : public StatementNode {
public:
  ExprPtr value; /* optional for void/u0 return */
  ReturnStmtNode(const Token* tok, size_t sc, ExprPtr val)
      : StatementNode(tok, sc), value(val) {}
  void accept(Visitor& v) override;
};

class FreeStmtNode : public StatementNode {
public:
  bool is_array_deallocation;
  ExprPtr expression;
  FreeStmtNode(const Token* tok, size_t sc, bool is_arr, ExprPtr expr)
      : StatementNode(tok, sc), is_array_deallocation(is_arr), expression(expr) {}
  void accept(Visitor& v) override;
};

class ErrorStmtNode : public StatementNode {
public:
  std::string message_content;
  ErrorStmtNode(const Token* tok, size_t sc, std::string msg)
      : StatementNode(tok, sc), message_content(std::move(msg)) {}
  void accept(Visitor& v) override;
};

class ExitStmtNode : public StatementNode {
public:
  int exit_code;
  ExitStmtNode(const Token* tok, size_t sc, int code)
      : StatementNode(tok, sc), exit_code(code) {}
  void accept(Visitor& v) override;
};

class AsmBlockNode : public StatementNode {
public:
  std::string body;
  AsmBlockNode(const Token* tok, size_t sc, std::string b)
      : StatementNode(tok, sc), body(std::move(b)) {}
  void accept(Visitor& v) override;
};

// == Declaration Nodes ==
class StructFieldNode : public AstNode {
public:
  IdentPtr name;
  Type* type;
  StructFieldNode(const Token* tok, size_t sc, IdentPtr name, Type* tk)
      : AstNode(tok, sc), name(name), type(tk) {}
  void accept(Visitor& v) override;
};

class StructDeclNode : public AstNode {
public:
  Type* type; /* new type to be set after type creation in parser */
  std::vector<StructFieldPtr> fields; /* only fields, no methods */
  std::uint64_t struct_size; /* total struct size set by the type checker */
  StructDeclNode(const Token* tok, size_t sc, Type* type, std::vector<StructFieldPtr> f)
      : AstNode(tok, sc), type(type), fields(std::move(f)), struct_size(0) {}
  void accept(Visitor& v) override;
};

class ParamNode : public AstNode {
public:
  BorrowState modifier;
  IdentPtr name;
  Type* type;
  ParamNode(const Token* tok, size_t sc, BorrowState mod, IdentPtr name, Type* tk)
      : AstNode(tok, sc), modifier(mod), name(name), type(tk) {}
  void accept(Visitor& v) override;
};

class FunctionDeclNode : public AstNode {
public:
  bool is_extern;
  IdentPtr name;
  std::vector<ParamPtr> params;
  IdentPtr retvar_name; /* optional for u0 return type */
  Type* return_type;
  BlockPtr body; /* nullptr if this is an extern function */
  FunctionDeclNode(const Token* tok, size_t sc, bool is_ext, IdentPtr name,
                   std::vector<ParamPtr> ps, IdentPtr rt_n,
                   Type* rt, BlockPtr b)
      : AstNode(tok, sc), is_extern(is_ext), name(name), params(std::move(ps)),
        retvar_name(rt_n), return_type(rt), body(b) {}
  void accept(Visitor& v) override;
};

#endif // PARSER_AST_H
