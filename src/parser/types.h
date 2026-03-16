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
    std::string_view identifier;
    Named(std::string_view id) : identifier(id) {}

    friend bool operator==(const Named& a, const Named& b) {
      return a.identifier == b.identifier;
    }
  };

  struct ParamInfo {
    Type* type;
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

  // Public interface
  template <typename T> bool is() const { return std::holds_alternative<T>(m_storage); }
  template <typename T> const T& as() const { return std::get<T>(m_storage); }

  inline static const std::uint64_t PTR_SIZE = 8;

  // returns the string format that should be used within the language
  std::string to_string() const;
  size_t get_scope_id() const { return m_scope_id; }
  std::uint64_t get_byte_size() const { return m_bytes; }
  void set_byte_size(std::uint64_t size) { m_bytes = size; }

  // Constructors
  Type(Named n, size_t sc, std::uint64_t bytes = PTR_SIZE)
      : m_storage(n), m_scope_id(sc), m_bytes(bytes) {}
  Type(Function f, size_t sc)
      : m_storage(std::move(f)), m_scope_id(sc), m_bytes(PTR_SIZE) {}
  Type(Pointer p, size_t sc)
      : m_storage(p), m_scope_id(sc), m_bytes(PTR_SIZE) {}

  friend bool operator==(const Type& a, const Type& b) {
    return a.m_storage == b.m_storage;
  }

private:
  std::variant<Named, Function, Pointer, ErrorType> m_storage;
  size_t m_scope_id;
  std::uint64_t m_bytes;

  std::string to_string_recursive(std::vector<const Type*>& visited) const;
};

// == Variables ==
class Variable {
public:
  std::string_view name;
  Span span;
  BorrowState modifier;
  Type* type; // nullptr means it must be inferred
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
