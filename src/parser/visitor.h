#ifndef PARSER_VISITOR_H
#define PARSER_VISITOR_H

#include <iostream>
#include <sstream>
#include <string>

#include "ast.h"
#include "../util.h"

class Visitor {
public:
  virtual ~Visitor() = default;

  virtual void visit(PoisonExprNode& node) = 0;
  virtual void visit(PoisonStmtNode& node) = 0;

  // Expression Nodes
  virtual void visit(IntegerLiteralNode& node) = 0;
  virtual void visit(FloatLiteralNode& node) = 0;
  virtual void visit(StringLiteralNode& node) = 0;
  virtual void visit(CharLiteralNode& node) = 0;
  virtual void visit(BoolLiteralNode& node) = 0;
  virtual void visit(NullLiteralNode&) = 0;
  virtual void visit(IdentifierNode& node) = 0;
  virtual void visit(AssignmentNode& node) = 0;
  virtual void visit(ImplicitCastNode& node) = 0;
  virtual void visit(BinaryOpExprNode& node) = 0;
  virtual void visit(UnaryExprNode& node) = 0;
  virtual void visit(FunctionCallNode& node) = 0;
  virtual void visit(FieldAccessNode& node) = 0;
  virtual void visit(ArrayIndexNode& node) = 0;
  virtual void visit(GroupedExprNode& node) = 0;
  virtual void visit(StructLiteralNode& node) = 0;
  virtual void visit(NewExprNode& node) = 0;

  // Statement Nodes
  virtual void visit(VariableDeclNode& node) = 0;
  virtual void visit(BlockNode& node) = 0;
  virtual void visit(IfStmtNode& node) = 0;
  virtual void visit(ForStmtNode& node) = 0;
  virtual void visit(WhileStmtNode& node) = 0;
  virtual void visit(BreakStmtNode&) = 0;
  virtual void visit(ContinueStmtNode&) = 0;
  virtual void visit(SwitchStmtNode& node) = 0;
  virtual void visit(ReadStmtNode& node) = 0;
  virtual void visit(PrintStmtNode& node) = 0;
  virtual void visit(ExpressionStatementNode& node) = 0;
  virtual void visit(ReturnStmtNode& node) = 0;
  virtual void visit(FreeStmtNode& node) = 0;
  virtual void visit(ErrorStmtNode& node) = 0;
  virtual void visit(ExitStmtNode& node) = 0;
  virtual void visit(AsmBlockNode& node) = 0;

  // Other Nodes
  virtual void visit(ArgumentNode& node) = 0;
  virtual void visit(StructFieldInitializerNode& node) = 0;
  virtual void visit(CaseNode& node) = 0;
  virtual void visit(StructFieldNode& node) = 0;
  virtual void visit(ParamNode& node) = 0;
  virtual void visit(FunctionDeclNode& node) = 0;
  virtual void visit(StructDeclNode& node) = 0;
};

void print_ast(const AstPtr& node, std::ostream& out);
void print_ast(const std::vector<AstPtr>& nodes, std::ostream& out);

class AstPrinter : public Visitor {
  std::ostream& out;
  int indent = 0;

  void print_indent();
  void print_type(const Type& type);

public:
  AstPrinter(std::ostream& out) : out(out) {}

  static std::string bin_op_to_string(BinOperator op);
  static std::string unary_op_to_string(UnaryOperator op);
  static std::string borrow_state_to_string(BorrowState bs);

  void visit(PoisonExprNode&) override {
    print_indent();
    out << "PoisonExpr()";
  }

  void visit(PoisonStmtNode&) override {
    print_indent();
    out << "PoisonStmt()";
  }

  void visit(IntegerLiteralNode& node) override {
    print_indent();
    out << "Int(" << node.value << ")";
  }

  void visit(FloatLiteralNode& node) override {
    print_indent();
    out << "Float(" << node.value << ")";
  }

  void visit(StringLiteralNode& node) override {
    print_indent();
    out << "String(\"" << escape_string(node.value) << "\")";
  }

  void visit(CharLiteralNode& node) override {
    print_indent();
    std::uint64_t val = node.value;
    out << "Char('" << static_cast<char>(val) << "' : " << val << ")";
  }

  void visit(BoolLiteralNode& node) override {
    print_indent();
    out << (node.value ? "true" : "false");
  }

  void visit(NullLiteralNode&) override {
    print_indent();
    out << "null";
  }

  void visit(IdentifierNode& node) override {
    print_indent();
    out << "Ident(" << node.name_str << ")";
  }

  void visit(BinaryOpExprNode& node) override {
    print_indent();
    out << "BinaryOp(" << bin_op_to_string(node.op_type) << ",\n";
    indent++;
    node.left->accept(*this);
    out << ",\n";
    node.right->accept(*this);
    indent--;
    out << "\n";
    print_indent();
    out << ")";
  }

  // Expression Nodes
  void visit(UnaryExprNode& node) override {
    print_indent();
    out << "UnaryOp(" << unary_op_to_string(node.op_type) << ",\n";
    indent++;
    node.operand->accept(*this);
    indent--;
    out << "\n";
    print_indent();
    out << ")";
  }

  void visit(FunctionCallNode& node) override {
    print_indent();
    out << "FunctionCall(\n";
    indent++;
    print_indent();
    out << "Callee:\n";
    indent++;
    node.callee->accept(*this);
    indent--;
    out << ",\n";
    print_indent();
    out << "Arguments: [\n";
    indent++;
    for (size_t i = 0; i < node.arguments.size(); ++i) {
      node.arguments[i]->accept(*this);
      if (i < node.arguments.size() - 1) {
        out << ",\n";
      }
    }
    indent--;
    out << "\n";
    print_indent();
    out << "]\n";
    indent--;
    print_indent();
    out << ")";
  }

  void visit(FieldAccessNode& node) override {
    print_indent();
    out << "FieldAccess(\n";
    indent++;
    print_indent();
    out << "Object:\n";
    indent++;
    node.object->accept(*this);
    indent--;
    out << ",\n";
    print_indent();
    out << "Field:\n";
    indent++;
    node.field->accept(*this);
    indent--;
    out << "\n";
    indent--;
    print_indent();
    out << ")";
  }

  void visit(ArrayIndexNode& node) override {
    print_indent();
    out << "ArrayIndex(\n";
    indent++;
    print_indent();
    out << "Object:\n";
    indent++;
    node.object->accept(*this);
    indent--;
    out << ",\n";
    print_indent();
    out << "Index:\n";
    indent++;
    node.index->accept(*this);
    indent--;
    out << "\n";
    indent--;
    print_indent();
    out << ")";
  }

  void visit(GroupedExprNode& node) override {
    print_indent();
    out << "GroupedExpr(\n";
    indent++;
    node.expression->accept(*this);
    indent--;
    out << "\n";
    print_indent();
    out << ")";
  }

  void visit(StructLiteralNode& node) override {
    print_indent();
    out << "StructLiteral(\n";
    indent++;
    print_indent();
    out << "Type:\n";
    indent++;
    print_type(*node.struct_decl->type);
    indent--;
    out << ",\n";
    print_indent();
    out << "Initializers: [\n";
    indent++;
    for (size_t i = 0; i < node.initializers.size(); ++i) {
      node.initializers[i]->accept(*this);
      if (i < node.initializers.size() - 1) {
        out << ",\n";
      }
    }
    indent--;
    out << "\n";
    print_indent();
    out << "]\n";
    indent--;
    print_indent();
    out << ")";
  }

  void visit(NewExprNode& node) override {
    print_indent();
    out << "NewExpr(\n";
    indent++;
    print_indent();
    out << "TypeToAllocate:";
    if (node.is_memory_mutable) {
      out << "(mutable memory, type:";
    } else {
      out << "(immutable memory, type:";
    }
    if (node.is_array_allocation) {
      out << " array)\n";
    } else {
      out << " constructor)\n";
    }
    indent++;
    print_type(*node.type_to_allocate);
    indent--;
    out << "\n";
    if (node.allocation_specifier) {
      print_indent();
      out << "AllocationSpecifier:\n";
      indent++;
      node.allocation_specifier->accept(*this);
      indent--;
      out << "\n";
    } else {
      print_indent();
      out << "AllocationSpecifier: null\n";
    }
    indent--;
    print_indent();
    out << ")";
  }

  // Statement Nodes
  void visit(VariableDeclNode& node) override {
    print_indent();
    out << "VariableDecl(Mutable: " << (node.is_mutable ? "true" : "false")
        << ",\n";
    indent++;
    print_indent();
    out << "Name:\n";
    indent++;
    node.name->accept(*this);
    indent--;
    out << ",\n";
    print_indent();
    out << "Type:";
    if (node.type != nullptr) {
      out << "\n";
      indent++;
      print_type(*node.type);
      indent--;
      out << "\n";
    } else {
      out << " inferred\n";
    }
    if (node.initializer) {
      print_indent();
      out << "Initializer:\n";
      indent++;
      node.initializer->accept(*this);
      indent--;
      out << "\n";
    } else {
      print_indent();
      out << "Initializer: null\n";
    }
    indent--;
    print_indent();
    out << ")";
  }

  void visit(AssignmentNode& node) override {
    print_indent();
    out << "Assignment(\n";
    indent++;
    print_indent();
    out << "LValue:\n";
    indent++;
    node.lvalue->accept(*this);
    indent--;
    out << ",\n";
    print_indent();
    out << "RValue:\n";
    indent++;
    node.rvalue->accept(*this);
    indent--;
    out << "\n";
    indent--;
    print_indent();
    out << ")";
  }

  void visit(ImplicitCastNode& node) override {
    out << "ImplicitCastNode(to: " << node.target_type->to_string() << ")\n";
    indent++;
    print_indent();
    node.expression->accept(*this);
    indent--;
  }

  void visit(BlockNode& node) override {
    print_indent();
    out << "Block([\n";
    indent++;
    for (size_t i = 0; i < node.statements.size(); ++i) {
      node.statements[i]->accept(*this);
      if (i < node.statements.size() - 1) {
        out << ",";
      }
      out << "\n";
    }
    indent--;
    print_indent();
    out << "])";
  }

  void visit(IfStmtNode& node) override {
    print_indent();
    out << "IfStmt(\n";
    indent++;
    print_indent();
    out << "Condition:\n";
    indent++;
    node.condition->accept(*this);
    indent--;
    out << ",\n";
    print_indent();
    out << "ThenBranch:\n";
    node.then_branch->accept(*this);
    if (node.else_branch) {
      out << ",\n";
      print_indent();
      out << "ElseBranch:\n";
      node.else_branch->accept(*this);
    }
    out << "\n";
    indent--;
    print_indent();
    out << ")";
  }

  void visit(ForStmtNode& node) override {
    print_indent();
    out << "ForStmt(\n";
    indent++;
    print_indent();
    out << "Initializer:";
    if (node.initializer != std::nullopt) {
      out << "\n";
      indent++;
      if (std::holds_alternative<ExprPtr>(*node.initializer)) {
        std::get<ExprPtr>(*node.initializer)->accept(*this);
      } else if (std::holds_alternative<StmtPtr>(*node.initializer)) {
        std::get<StmtPtr>(*node.initializer)->accept(*this);
      } else {
        out << " null"; // it should be either an ExprPtr or StmtPtr
      }
      indent--;
    } else {
      out << " null";
    }
    out << ",\n";
    print_indent();
    out << "Condition:";
    if (node.condition) {
      out << "\n";
      indent++;
      node.condition->accept(*this);
      indent--;
    } else {
      out << "null";
    }
    out << ",\n";
    print_indent();
    out << "Iteration:";
    if (node.iteration) {
      out << "\n";
      indent++;
      node.iteration->accept(*this);
      indent--;
    } else {
      out << "null";
    }
    out << ",\n";
    print_indent();
    out << "Body:\n";
    node.body->accept(*this);
    out << "\n";
    indent--;
    print_indent();
    out << ")";
  }

  void visit(WhileStmtNode& node) override {
    print_indent();
    out << "WhileStmt(\n";
    indent++;
    print_indent();
    out << "Condition:\n";
    indent++;
    node.condition->accept(*this);
    indent--;
    out << ",\n";
    print_indent();
    out << "Body:\n";
    node.body->accept(*this);
    out << "\n";
    indent--;
    print_indent();
    out << ")";
  }

  void visit(BreakStmtNode&) override {
    print_indent();
    out << "BreakStmt";
  }

  void visit(ContinueStmtNode&) override {
    print_indent();
    out << "ContinueStmt";
  }

  void visit(SwitchStmtNode& node) override {
    print_indent();
    out << "SwitchStmt(\n";
    indent++;
    print_indent();
    out << "Expression:\n";
    indent++;
    node.expression->accept(*this);
    indent--;
    out << ",\n";
    print_indent();
    out << "Cases: [\n";
    indent++;
    for (size_t i = 0; i < node.cases.size(); ++i) {
      node.cases[i]->accept(*this);
      if (i < node.cases.size() - 1) {
        out << ",";
      }
      out << "\n";
    }
    indent--;
    print_indent();
    out << "]\n";
    indent--;
    print_indent();
    out << ")";
  }

  void visit(ReadStmtNode& node) override {
    print_indent();
    out << "ReadStmt(\n";
    indent++;
    node.expression->accept(*this);
    indent--;
    out << "\n";
    print_indent();
    out << ")";
  }

  void visit(PrintStmtNode& node) override {
    print_indent();
    out << "PrintStmt(\n";
    indent++;
    for (size_t i = 0; i < node.expressions.size(); ++i) {
      node.expressions[i]->accept(*this);
      out << "\n";
    }
    indent--;
    print_indent();
    out << ")";
  }

  void visit(ExpressionStatementNode& node) override {
    print_indent();
    out << "ExpressionStmt(\n";
    indent++;
    node.expression->accept(*this);
    indent--;
    out << "\n";
    print_indent();
    out << ")";
  }

  void visit(ReturnStmtNode& node) override {
    print_indent();
    out << "ReturnStmt(";
    if (node.value) {
      out << "\n";
      indent++;
      node.value->accept(*this);
      indent--;
      out << "\n";
      print_indent();
    }
    out << ")";
  }

  void visit(FreeStmtNode& node) override {
    print_indent();
    out << "FreeStmt(IsArray: "
        << (node.is_array_deallocation ? "true" : "false") << ",\n";
    indent++;
    node.expression->accept(*this);
    indent--;
    out << "\n";
    print_indent();
    out << ")";
  }

  void visit(ErrorStmtNode& node) override {
    print_indent();
    out << "ErrorStmt(Message: \"" << node.message_content << "\")";
  }

  void visit(ExitStmtNode& node) override {
    print_indent();
    out << "ExitStmt(exit code: " << node.exit_code << ")";
  }

  void visit(AsmBlockNode& node) override {
    print_indent();
    out << "AsmBlock(\n";
    indent++;
    print_indent();
    out << "Body: \"\"\"\n";
    std::istringstream iss(node.body);
    std::string line;
    while (std::getline(iss, line)) {
      print_indent();
      out << line << "\n";
    }
    print_indent();
    out << "\"\"\"\n";
    indent--;
    print_indent();
    out << ")";
  }

  // Other Nodes
  void visit(ArgumentNode& node) override {
    print_indent();
    out << "Argument(IsGive: " << (node.is_give ? "true" : "false") << ",\n";
    indent++;
    node.expression->accept(*this);
    indent--;
    out << "\n";
    print_indent();
    out << ")";
  }

  void visit(StructFieldInitializerNode& node) override {
    print_indent();
    out << "StructFieldInitializer(\n";
    indent++;
    print_indent();
    out << "Field:\n";
    indent++;
    node.field->accept(*this);
    indent--;
    out << ",\n";
    print_indent();
    out << "Value:\n";
    indent++;
    node.value->accept(*this);
    indent--;
    out << "\n";
    indent--;
    print_indent();
    out << ")";
  }

  void visit(CaseNode& node) override {
    print_indent();
    out << "Case(\n";
    indent++;
    print_indent();
    out << "Value:";
    if (node.value) {
      out << "\n";
      indent++;
      node.value->accept(*this);
      indent--;
    } else {
      out << "default";
    }
    out << ",\n";
    print_indent();
    out << "Body:\n";
    node.body->accept(*this);
    out << "\n";
    indent--;
    print_indent();
    out << ")";
  }

  void visit(StructFieldNode& node) override {
    print_indent();
    out << "StructField(\n";
    indent++;
    print_indent();
    out << "Name:\n";
    indent++;
    node.name->accept(*this);
    indent--;
    out << ",\n";
    print_indent();
    out << "Type:\n";
    indent++;
    print_type(*node.type);
    indent--;
    out << "\n";
    indent--;
    print_indent();
    out << ")";
  }

  void visit(ParamNode& node) override {
    print_indent();
    out << "Param(Modifier: " << borrow_state_to_string(node.modifier) << ",\n";
    indent++;
    print_indent();
    out << "Name:\n";
    indent++;
    node.name->accept(*this);
    indent--;
    out << ",\n";
    print_indent();
    out << "Type:\n";
    indent++;
    print_type(*node.type);
    indent--;
    out << "\n";
    indent--;
    print_indent();
    out << ")";
  }

  void visit(FunctionDeclNode& node) override {
    print_indent();
    out << "FunctionDecl(\n";
    indent++;
    print_indent();
    out << "Name:\n";
    indent++;
    node.name->accept(*this);
    indent--;
    out << ",\n";
    print_indent();
    out << "Params: [\n";
    indent++;
    for (size_t i = 0; i < node.params.size(); ++i) {
      node.params[i]->accept(*this);
      if (i < node.params.size() - 1) {
        out << ",";
      }
      out << "\n";
    }
    indent--;
    print_indent();
    out << "],\n";
    print_indent();
    out << "ReturnType:";
    if (node.retvar_name) {
      out << " (NamedVar:" << node.retvar_name->name_str << ")";
    }
    out << '\n';
    indent++;
    print_type(*node.return_type);
    indent--;
    out << ",\n";
    print_indent();
    out << "Body:\n";
    node.body->accept(*this);
    out << "\n";
    indent--;
    print_indent();
    out << ")";
  }

  void visit(StructDeclNode& node) override {
    print_indent();
    out << "StructDecl(\n";
    indent++;
    print_indent();
    out << "Type:\n";
    indent++;
    print_type(*node.type);
    indent--;
    out << ",\n";
    print_indent();
    out << "Fields: [\n";
    indent++;
    size_t size = node.fields.size();
    for (size_t i = 0; i < size; ++i) {
      node.fields[i]->accept(*this);
      if (i < size - 1) {
        out << ",";
      }
      out << "\n";
    }
    indent--;
    print_indent();
    out << "]\n";
    indent--;
    print_indent();
    out << ")";
  }
};

#endif
