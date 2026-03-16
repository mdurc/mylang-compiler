#include "symtab.h"

#include <map>

std::string variable_borrowed_state_to_string(BorrowState bs) {
  switch (bs) {
    case BorrowState::MutablyOwned:
    case BorrowState::MutablyBorrowed: return "mut";
    case BorrowState::ImmutablyOwned:
    case BorrowState::ImmutablyBorrowed: return "imm";
    default: return "";
  }
}

const Symbol* Scope::lookup(std::string_view name) const {
  auto it = m_symbols.find(name);
  return it != m_symbols.end() ? &it->second : nullptr;
}

StructDeclPtr Scope::lookup_struct(std::string_view name) const {
  auto it = m_structs.find(name);
  return it != m_structs.end() ? it->second : nullptr;
}

void Scope::print(std::ostream& out, const std::string& indent) const {
  out << indent << "Scope (parent: " << m_parent_scope << ")\n";
  if (m_symbols.empty()) {
    return;
  }
  out << indent << "Symbols:\n";
  // sort the symbols for the snapshot testing
  std::map<std::string, Symbol> sorted_syms(m_symbols.begin(), m_symbols.end());
  for (const auto& [name, symbol] : sorted_syms) {
    out << indent << "  " << name << ": ";
    switch (symbol.kind) {
      case Symbol::Variable: {
        auto var = symbol.as<Variable>();
        out << "var " << var->name
            << " (type: " << (var->type ? var->type->to_string() : "?")
            << ", scope: " << var->scope_id << ", modifier: "
            << variable_borrowed_state_to_string(var->modifier) << ")";
        break;
      }
      case Symbol::Type: {
        auto type = symbol.as<Type>();
        out << "type " << type->to_string();
        break;
      }
    }
    out << "\n";
  }
}

SymTab::SymTab(ArenaAllocator* arena) : m_current_scope(0) {
  m_scopes.emplace_back(0); // global scope

  // define the primitives in the language
  declare_type("u0", arena->make<Type>(Type::Named("u0"), 0, 8), 0);
  declare_type("u8", arena->make<Type>(Type::Named("u8"), 0, 1), 0);
  declare_type("u16", arena->make<Type>(Type::Named("u16"), 0, 2), 0);
  declare_type("u32", arena->make<Type>(Type::Named("u32"), 0, 4), 0);
  declare_type("u64", arena->make<Type>(Type::Named("u64"), 0, 8), 0);
  declare_type("i8", arena->make<Type>(Type::Named("i8"), 0, 1), 0);
  declare_type("i16", arena->make<Type>(Type::Named("i16"), 0, 2), 0);
  declare_type("i32", arena->make<Type>(Type::Named("i32"), 0, 4), 0);
  declare_type("i64", arena->make<Type>(Type::Named("i64"), 0, 8), 0);
  declare_type("f64", arena->make<Type>(Type::Named("f64"), 0, 8), 0);
  declare_type("bool", arena->make<Type>(Type::Named("bool"), 0, 1), 0);
  declare_type("string", arena->make<Type>(Type::Named("string"), 0), 0);
}

void SymTab::enter_scope() {
  m_scopes.emplace_back(m_current_scope);
  m_current_scope = m_scopes.size() - 1;
}

void SymTab::exit_scope() {
  if (m_current_scope != 0) {
    m_current_scope = m_scopes[m_current_scope].get_parent_scope();
  }
}

void SymTab::print(std::ostream& out) const {
  out << "Symbol Table State:\n";
  out << "Current Scope ID: " << m_current_scope << "\n";
  out << "Total Scopes: " << m_scopes.size() << "\n\n";
  for (size_t i = 0; i < m_scopes.size(); ++i) {
    out << "Scope ID: " << i << "\n";
    m_scopes[i].print(out, "  ");
    out << "\n";
  }
}
