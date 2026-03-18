#ifndef CHECKER_TYPECHECKER_H
#define CHECKER_TYPECHECKER_H

#include "../logging/logger.h"
#include "../memory/arena.h"
#include "../parser/ast.h"
#include "../parser/symtab.h"
#include "../parser/visitor.h"

class TypeChecker : public Visitor {
public:
  TypeChecker(Logger* logger, ArenaAllocator* arena);

  void check_program(SymTab* symtab, const std::vector<AstPtr>& program_nodes);

  // For ExpressionNode derivatives, they will compute and store the type.
  // For StatementNode derivatives, they will enforce type rules.

  void visit(PoisonExprNode& node) override;
  void visit(PoisonStmtNode& node) override;

  void visit(IntegerLiteralNode& node) override;
  void visit(FloatLiteralNode& node) override;
  void visit(StringLiteralNode& node) override;
  void visit(CharLiteralNode& node) override;
  void visit(BoolLiteralNode& node) override;
  void visit(NullLiteralNode& node) override;
  void visit(IdentifierNode& node) override;
  void visit(AssignmentNode& node) override;
  void visit(ImplicitCastNode& node) override;
  void visit(BinaryOpExprNode& node) override;
  void visit(UnaryExprNode& node) override;
  void visit(FunctionCallNode& node) override;
  void visit(FieldAccessNode& node) override;
  void visit(ArrayIndexNode& node) override;
  void visit(GroupedExprNode& node) override;
  void visit(StructLiteralNode& node) override;
  void visit(NewExprNode& node) override;

  void visit(VariableDeclNode& node) override;
  void visit(BlockNode& node) override;
  void visit(IfStmtNode& node) override;
  void visit(ForStmtNode& node) override;
  void visit(WhileStmtNode& node) override;
  void visit(BreakStmtNode& node) override;
  void visit(ContinueStmtNode& node) override;
  void visit(SwitchStmtNode& node) override;
  void visit(ReadStmtNode& node) override;
  void visit(PrintStmtNode& node) override;
  void visit(ExpressionStatementNode& node) override;
  void visit(ReturnStmtNode& node) override;
  void visit(FreeStmtNode& node) override;
  void visit(ErrorStmtNode& node) override;
  void visit(ExitStmtNode& node) override;
  void visit(AsmBlockNode& node) override;

  void visit(CaseNode& node) override;
  void visit(ArgumentNode& node) override;
  void visit(StructFieldInitializerNode& node) override;
  void visit(StructFieldNode& node) override;
  void visit(ParamNode& node) override;
  void visit(FunctionDeclNode& node) override;
  void visit(StructDeclNode& node) override;

private:
  Logger* m_logger;
  ArenaAllocator* m_arena;
  SymTab* m_symtab;

  /* for return types */
  Type* m_current_function_return_type;

  /* for break/continue */
  bool m_in_loop;

  /* to check case stmt compatibility */
  Type* m_current_switch_expr_type;

  /* calls expr->accept(*this) which will return a resolved type */
  Type* get_expr_type(const ExprPtr& expr);

  /* type compatibility and error reporting helpers */
  bool check_type_assignable(Type* target_type, Type* value_type, const Span& span);
  void try_apply_implicit_cast(ExprPtr& expr, Type* target_type);
  bool expect_specific_type(const ExprPtr& expr, const Type& expected_type_val);
  bool expect_boolean_type(const ExprPtr& expr);

  /* Helper to determine if an expression is a mutable l-value.
     Returns the type of the l-value if it is mutable, nullptr otherwise. */
  Type* get_lvalue_type_if_mutable(const ExprPtr& expr);

  bool is_integer_type(Type* type) const;
  bool is_numeric_type(Type* type) const;
  bool is_primitive_type(Type* type) const;
};

#endif // CHECKER_TYPECHECKER_H
