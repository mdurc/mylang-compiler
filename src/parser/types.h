#ifndef PARSER_TYPES_H
#define PARSER_TYPES_H

#include "../lexer/token.h"

// == Type System ==

enum BorrowState {
  MutablyOwned,      // as param: take mut
  ImmutablyOwned,    // as param: take | take imm
  MutablyBorrowed,   // as param: mut
  ImmutablyBorrowed, // as param: imm
};

class Type {
public:
  struct Named {
    std::string_view identifier; /* points to name allocated for symbol table */
    Named(std::string_view id) : identifier(id) {}

    friend bool operator==(const Named& a, const Named& b) {
      return a.identifier == b.identifier;
    }
  };

  struct ParamInfo {
    Type* type; /* points to type defined by symbol table */
    BorrowState modifier;

    friend bool operator==(const ParamInfo& a, const ParamInfo& b) {
      if (!a.type || !b.type) return a.type == b.type;
      return a.type == b.type && a.modifier == b.modifier;
    }
  };

  struct Function {
    std::vector<ParamInfo> params;
    Type* return_type;

    Function(std::vector<ParamInfo> params, Type* ret_type)
        : params(std::move(params)), return_type(ret_type) {}

    friend bool operator==(const Function& a, const Function& b) {
      return a.return_type == b.return_type && a.params == b.params;
    }
  };

  struct Pointer {
    bool is_pointee_mutable;
    Type* pointee;

    Pointer(bool is_pointee_mutable, Type* pointee)
        : is_pointee_mutable(is_pointee_mutable), pointee(pointee) {}

    friend bool operator==(const Pointer& a, const Pointer& b) {
      return a.is_pointee_mutable == b.is_pointee_mutable &&
             a.pointee == b.pointee;
    }
  };

  struct ErrorType {
    friend bool operator==(const ErrorType&, const ErrorType&) { return true; }
  };

  // public interface
  inline static const std::uint64_t PTR_SIZE = 8;

  /* interpret a Type* t via: t->is<Function>() */
  template <typename T> bool is() const { return std::holds_alternative<T>(m_storage); }
  template <typename T> const T& as() const { return std::get<T>(m_storage); }

  /* returns the string format that should be used within the language */
  std::string to_string() const;

  /* types are scoped based on declaration scope */
  size_t get_scope_id() const { return m_scope_id; }

  /* getter for ir/code generation */
  std::uint64_t get_byte_size() const { return m_bytes; }
  void set_byte_size(std::uint64_t sz) { m_bytes = sz; }

  std::uint64_t get_alignment() const { return m_alignment; }
  void set_alignment(std::uint64_t align) { m_alignment = align; }

  bool is_aggregate() const { return m_is_aggregate; }
  void set_aggregate(bool is_agg) { m_is_aggregate = is_agg; }

  Type(Named n, size_t sc, std::uint64_t bytes, std::uint64_t align = 0)
      : m_storage(n), m_scope_id(sc), m_bytes(bytes),
        m_alignment(align > 0 ? align : bytes), m_is_aggregate(false){}
  Type(Function f, size_t sc)
      : m_storage(std::move(f)), m_scope_id(sc), m_bytes(PTR_SIZE),
        m_alignment(PTR_SIZE), m_is_aggregate(false) {}
  Type(Pointer p, size_t sc)
      : m_storage(p), m_scope_id(sc), m_bytes(PTR_SIZE),
        m_alignment(PTR_SIZE), m_is_aggregate(false) {}
  Type(ErrorType e, size_t sc)
      : m_storage(e), m_scope_id(sc), m_bytes(PTR_SIZE),
        m_alignment(PTR_SIZE), m_is_aggregate(false) {}

  friend bool operator==(const Type& a, const Type& b) {
    return a.m_storage == b.m_storage;
  }

private:
  std::variant<Named, Function, Pointer, ErrorType> m_storage;
  size_t m_scope_id;
  std::uint64_t m_bytes;
  std::uint64_t m_alignment;
  bool m_is_aggregate;

  std::string to_string_recursive(std::vector<const Type*>& visited) const;
};

// == Variables ==
class Variable {
public:
  std::string_view name; /* points to name allocated for symbol table */
  Span span; /* span at variable declaration */

  BorrowState modifier;
  Type* type; /* nullptr means it must be inferred by the type-checker */
  size_t scope_id;
  bool is_return_var;

  Variable(std::string_view name, const Span& span, BorrowState mod,
           Type* tk, size_t sc, bool ret_var = false)
      : name(name), span(span), modifier(mod), type(tk),
        scope_id(sc), is_return_var(ret_var) {}

  friend bool operator==(const Variable& a, const Variable& b) {
    return a.name == b.name;
  }
};

#endif // PARSER_TYPES_H
