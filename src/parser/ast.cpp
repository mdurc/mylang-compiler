#include "ast.h"

#include "visitor.h"

void PoisonExprNode::accept(Visitor& v) { v.visit(*this); }
void PoisonStmtNode::accept(Visitor& v) { v.visit(*this); }
void IntegerLiteralNode::accept(Visitor& v) { v.visit(*this); }
void FloatLiteralNode::accept(Visitor& v) { v.visit(*this); }
void StringLiteralNode::accept(Visitor& v) { v.visit(*this); }
void CharLiteralNode::accept(Visitor& v) { v.visit(*this); }
void BoolLiteralNode::accept(Visitor& v) { v.visit(*this); }
void NullLiteralNode::accept(Visitor& v) { v.visit(*this); }
void BreakStmtNode::accept(Visitor& v) { v.visit(*this); }
void ContinueStmtNode::accept(Visitor& v) { v.visit(*this); }
void IdentifierNode::accept(Visitor& v) { v.visit(*this); }
void AssignmentNode::accept(Visitor& v) { v.visit(*this); }
void ImplicitCastNode::accept(Visitor& v) { v.visit(*this); }
void ExplicitCastNode::accept(Visitor& v) { v.visit(*this); }
void SizeOfNode::accept(Visitor& v) { v.visit(*this); }
void BinaryOpExprNode::accept(Visitor& v) { v.visit(*this); }
void UnaryExprNode::accept(Visitor& v) { v.visit(*this); }
void ArgumentNode::accept(Visitor& v) { v.visit(*this); }
void FunctionCallNode::accept(Visitor& v) { v.visit(*this); }
void FieldAccessNode::accept(Visitor& v) { v.visit(*this); }
void ArrayIndexNode::accept(Visitor& v) { v.visit(*this); }
void GroupedExprNode::accept(Visitor& v) { v.visit(*this); }
void StructFieldInitializerNode::accept(Visitor& v) { v.visit(*this); }
void StructLiteralNode::accept(Visitor& v) { v.visit(*this); }
void NewExprNode::accept(Visitor& v) { v.visit(*this); }
void VariableDeclNode::accept(Visitor& v) { v.visit(*this); }
void BlockNode::accept(Visitor& v) { v.visit(*this); }
void IfStmtNode::accept(Visitor& v) { v.visit(*this); }
void ForStmtNode::accept(Visitor& v) { v.visit(*this); }
void WhileStmtNode::accept(Visitor& v) { v.visit(*this); }
void CaseNode::accept(Visitor& v) { v.visit(*this); }
void SwitchStmtNode::accept(Visitor& v) { v.visit(*this); }
void ReadStmtNode::accept(Visitor& v) { v.visit(*this); }
void PrintStmtNode::accept(Visitor& v) { v.visit(*this); }
void ExpressionStatementNode::accept(Visitor& v) { v.visit(*this); }
void ReturnStmtNode::accept(Visitor& v) { v.visit(*this); }
void FreeStmtNode::accept(Visitor& v) { v.visit(*this); }
void ErrorStmtNode::accept(Visitor& v) { v.visit(*this); }
void ExitStmtNode::accept(Visitor& v) { v.visit(*this); }
void AsmBlockNode::accept(Visitor& v) { v.visit(*this); }
void StructFieldNode::accept(Visitor& v) { v.visit(*this); }
void StructDeclNode::accept(Visitor& v) { v.visit(*this); }
void ParamNode::accept(Visitor& v) { v.visit(*this); }
void FunctionDeclNode::accept(Visitor& v) { v.visit(*this); }
