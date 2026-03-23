#ifndef LEXER_TOKEN_H
#define LEXER_TOKEN_H

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

#include "span.h"

enum class TokenType {
  // Keywords:
  FUNC,     // func
  EXTERN,   // extern
  IF,       // if
  ELSE,     // else
  FOR,      // for
  WHILE,    // while
  RETURN,   // return
  RETURNS,  // returns
  BREAK,    // break
  CONTINUE, // continue
  TRUE,     // true
  FALSE,    // false
  NULL_,    // null
  AND,      // and
  OR,       // or
  STRUCT,   // struct
  SWITCH,   // switch
  CASE,     // case
  DEFAULT,  // default
  IMM,      // imm
  MUT,      // mut
  TAKE,     // take
  GIVE,     // give
  PTR,      // ptr
  NEW,      // new
  CAST,     // cast
  FREE,     // free
  ASM,      // asm
  ERROR_KW, // error
  EXIT_KW,  // exit
  READ,     // read
  PRINT,    // print

  IDENTIFIER,

  // Primitive type
  U0,     // u0
  U8,     // u8
  U16,    // u16
  U32,    // u32
  U64,    // u64
  I8,     // i8
  I16,    // i16
  I32,    // i32
  I64,    // i64
  F64,    // f64
  BOOL,   // bool
  STRING, // string

  // Literals
  INT_LITERAL,
  FLOAT_LITERAL,
  STRING_LITERAL,
  CHAR_LITERAL,

  // Syntax
  LPAREN,    // (
  RPAREN,    // )
  LBRACE,    // {
  RBRACE,    // }
  LBRACK,    // [
  RBRACK,    // ]
  LANGLE,    // <
  RANGLE,    // >
  COMMA,     // ,
  COLON,     // :
  SEMICOLON, // ;
  DOT,       // .
  HASH,      // #

  // Operators
  BANG,      // !
  PLUS,      // +
  MINUS,     // -
  SLASH,     // /
  STAR,      // *
  EQUAL,     // =
  AMPERSAND, // &
  PIPE,      // |
  CARET,     // ^
  MODULO,    // %

  // Compound operators:
  WALRUS,          // :=
  EQUAL_EQUAL,     // ==
  BANG_EQUAL,      // !=
  LESS_EQUAL,      // <=
  GREATER_EQUAL,   // >=
  LESS_LESS,       // <<
  GREATER_GREATER, // >>
  ARROW,           // ->

  UNKNOWN,
  EOF_TOK,
};

std::string token_type_to_string(TokenType type);
bool is_basic_type(TokenType type);

class Token {
public:
  // default constructable variant
  using Lit = std::variant<std::monostate, std::uint64_t, std::string, double>;

  Token(TokenType type, std::string_view lexeme, Span span, Lit value = std::monostate{})
      : m_type(type), m_lexeme(lexeme), m_span(span), m_literal_value(std::move(value)) {}

  TokenType get_type() const { return m_type; }
  const Span& get_span() const { return m_span; }

  std::string_view get_lexeme() const { return m_lexeme; }

  bool is_literal() const { return !std::holds_alternative<std::monostate>(m_literal_value); }
  std::uint64_t get_int_val() const { return std::get<std::uint64_t>(m_literal_value); }
  const std::string& get_string_val() const { return std::get<std::string>(m_literal_value); }
  double get_float_val() const { return std::get<double>(m_literal_value); }

  static void print_tokens(const std::vector<Token>& toks, std::ostream& out);
  friend std::ostream& operator<<(std::ostream& out, const Token& token);

private:
  TokenType m_type;
  std::string m_lexeme;
  Span m_span;
  Lit m_literal_value;
};


#endif // LEXER_TOKEN_H
