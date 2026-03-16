#include "visitor.h"

void AstPrinter::print_indent() {
  for (int i = 0; i < indent; ++i) out << "  ";
}

std::string AstPrinter::bin_op_to_string(BinOperator op) {
  switch (op) {
    case BinOperator::Plus: return "+";
    case BinOperator::Minus: return "-";
    case BinOperator::Divide: return "/";
    case BinOperator::Modulo: return "%";
    case BinOperator::Multiply: return "*";
    case BinOperator::Equal: return "==";
    case BinOperator::NotEqual: return "!=";
    case BinOperator::GreaterThan: return ">";
    case BinOperator::GreaterEqual: return ">=";
    case BinOperator::LessThan: return "<";
    case BinOperator::LessEqual: return "<=";
    case BinOperator::LogicalAnd: return "&&";
    case BinOperator::LogicalOr: return "||";
    default: return "?";
  }
}

std::string AstPrinter::unary_op_to_string(UnaryOperator op) {
  switch (op) {
    case UnaryOperator::Negate: return "-";
    case UnaryOperator::LogicalNot: return "!";
    case UnaryOperator::Dereference: return "*";
    case UnaryOperator::AddressOf: return "&";
    case UnaryOperator::AddressOfMut: return "&mut";
    default: return "?";
  }
}

std::string AstPrinter::borrow_state_to_string(BorrowState bs) {
  switch (bs) {
    case BorrowState::MutablyOwned: return "MutablyOwned";
    case BorrowState::ImmutablyOwned: return "ImmutablyOwned";
    case BorrowState::MutablyBorrowed: return "MutablyBorrowed";
    case BorrowState::ImmutablyBorrowed: return "ImmutablyBorrowed";
    default: return "UnknownBorrowState";
  }
}

void AstPrinter::print_type(const Type& type) {
  // This will print an AST view of the type, on top of printing the type in the
  // expected type format from the Type class implementation
  print_indent();
  out << "{" << type.to_string() << "} ";
  if (type.is<Type::Named>()) {
    // Named Types
    out << "Type::Named(" << type.as<Type::Named>().identifier << ")";
  } else if (type.is<Type::Function>()) {
    // Function Types
    out << "Type::Function(\n";
    indent++;
    print_indent();
    out << "Params: [\n";
    indent++;
    const Type::Function& func = type.as<Type::Function>();
    for (size_t i = 0; i < func.params.size(); ++i) {
      print_indent();
      out << "Modifier: " << borrow_state_to_string(func.params[i].modifier)
          << "\n";
      print_indent();
      out << "Type:\n";
      indent++;
      print_type(*func.params[i].type);
      indent--;
      out << "\n";
      print_indent();
      out << ")";
      if (i < func.params.size() - 1) out << ",";
      out << "\n";
    }
    indent--;
    print_indent();
    out << "],\n";
    print_indent();
    out << "Return Type:\n";
    indent++;
    print_type(*func.return_type);
    indent--;
    out << "\n";
    indent--;
    print_indent();
    out << ")";
  } else if (type.is<Type::Pointer>()) {
    // Ptr types
    const Type::Pointer& ptr = type.as<Type::Pointer>();
    out << "Type::Pointer(pointee mutable: "
        << (ptr.is_pointee_mutable ? "true" : "false") << ",\n";
    indent++;
    print_indent();
    out << "Pointee:\n";
    indent++;
    print_type(*ptr.pointee);
    indent--;
    out << "\n";
    indent--;
    print_indent();
    out << ")";
  } else if (type.is<Type::ErrorType>()) {
    out << "ErrorType!";
  } else {
    out << "UnknownType";
  }
}

void print_ast(const AstPtr& node, std::ostream& out) {
  AstPrinter printer(out);
  node->accept(printer);
  out << std::endl;
}

void print_ast(const std::vector<AstPtr>& nodes, std::ostream& out) {
  AstPrinter printer(out);
  for (const AstPtr& node : nodes) {
    node->accept(printer);
    out << std::endl;
  }
}
