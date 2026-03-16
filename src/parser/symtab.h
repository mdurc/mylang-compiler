#ifndef PARSER_SYMTAB_H
#define PARSER_SYMTAB_H

#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "ast.h"
#include "../memory/arena.h"
#include "types.h"

std::string variable_borrowed_state_to_string(BorrowState bs);
using SymbolData = std::variant<std::monostate, Variable*, Type*>;

struct Symbol {
  enum Kind { Variable, Type };
  Kind kind;
  SymbolData data;

  template <typename T>
  T* as() const {
    if (std::holds_alternative<T*>(data)) {
      return std::get<T*>(data);
    }
    return nullptr;
  }
};

class Scope {
public:
  Scope(size_t parent) : m_parent_scope(parent) {}
  size_t get_parent_scope() const { return m_parent_scope; }

  const std::unordered_map<std::string_view, Symbol>& get_symbols() const {
    return m_symbols;
  }

  Variable* add_variable(std::string_view name, Variable* var) {
    auto itr = m_symbols.insert({name, {Symbol::Variable, var}});
    return itr.second ? var : nullptr;
  }

  Type* add_type(std::string_view name, Type* type) {
    auto itr = m_symbols.insert({name, {Symbol::Type, type}});
    return itr.second ? type : nullptr;
  }

  StructDeclPtr add_struct(std::string_view name, StructDeclPtr decl) {
    auto itr = m_structs.insert({name, decl});
    return itr.second ? decl : nullptr;
  }

  const Symbol* lookup(std::string_view name) const;
  StructDeclPtr lookup_struct(std::string_view name) const;

  void print(std::ostream& out, const std::string& indent = "") const;

private:
  size_t m_parent_scope;
  std::unordered_map<std::string_view, Symbol> m_symbols;
  std::unordered_map<std::string_view, StructDeclPtr> m_structs;
};

class SymTab {
public:
  SymTab(ArenaAllocator* arena);

  void enter_scope();
  void exit_scope();

  const std::vector<Scope>& get_scopes() const { return m_scopes; }
  size_t current_scope() const { return m_current_scope; }

  template <typename T>
  T* lookup(std::string_view name, size_t start_scope) const {
    if (start_scope >= m_scopes.size()) {
      return nullptr;
    }
    for (size_t scope_id = start_scope;; scope_id = m_scopes[scope_id].get_parent_scope()) {
      const Symbol* s = m_scopes[scope_id].lookup(name);
      if (s) return s->as<T>();
      if (scope_id == 0) break;
    }
    return nullptr;
  }
  StructDeclPtr lookup_struct(std::string_view name, size_t start_scope) const;

  Variable* declare_variable(std::string_view name, Variable* var, size_t scope) {
    return m_scopes[scope].add_variable(name, var);
  }
  Type* declare_type(std::string_view name, Type* type, size_t scope) {
    return m_scopes[scope].add_type(name, type);
  }
  StructDeclPtr declare_struct(std::string_view name, StructDeclPtr decl, size_t scope) {
    return m_scopes[scope].add_struct(name, decl);
  }

  void print(std::ostream& out) const;

private:
  size_t m_current_scope;
  std::vector<Scope> m_scopes;
};

#endif // PARSER_SYMTAB_H
