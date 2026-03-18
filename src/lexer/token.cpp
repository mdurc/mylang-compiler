#include "token.h"

#include <iomanip>
#include <sstream>
#include "../util.h"

bool is_basic_type(TokenType type) {
  switch (type) {
    case TokenType::U0:
    case TokenType::U8:
    case TokenType::U16:
    case TokenType::U32:
    case TokenType::U64:
    case TokenType::I8:
    case TokenType::I16:
    case TokenType::I32:
    case TokenType::I64:
    case TokenType::F64:
    case TokenType::BOOL:
    case TokenType::STRING: return true;
    default: return false;
  }
}

void Token::print_tokens(const std::vector<Token>& toks, std::ostream& out) {
  out << "Tokens (" << toks.size() << "):\n";
  for (const Token& t : toks) {
    out << t << "\n";
  }
}

std::ostream& operator<<(std::ostream& out, const Token& token) {
  constexpr int type_width = 16;
  constexpr int lexeme_width = 16;
  constexpr int literal_width = 16;

  out << "[" << std::left << std::setw(type_width)
      << token_type_to_string(token.m_type) << "] ";

  out << std::left << std::setw(lexeme_width) << token.m_lexeme;

  if (token.is_literal()) {
    std::ostringstream literal_stream;
    std::visit(
        [&literal_stream](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, std::string>) {
          literal_stream << escape_string(arg);
          } else if constexpr (!std::is_same_v<T, std::monostate>) {
          literal_stream << arg;
          }
        },
        token.m_literal_value);
    out << std::left << std::setw(literal_width) << literal_stream.str();
  } else {
    out << std::left << std::setw(literal_width) << "";
  }

  out << "at " << token.m_span;
  return out;
}

std::string token_type_to_string(TokenType type) {
  switch (type) {
    // keywords
    case TokenType::FUNC: return "FUNC";
    case TokenType::IF: return "IF";
    case TokenType::ELSE: return "ELSE";
    case TokenType::FOR: return "FOR";
    case TokenType::WHILE: return "WHILE";
    case TokenType::READ: return "READ";
    case TokenType::PRINT: return "PRINT";
    case TokenType::RETURN: return "RETURN";
    case TokenType::RETURNS: return "RETURNS";
    case TokenType::BREAK: return "BREAK";
    case TokenType::CONTINUE: return "CONTINUE";
    case TokenType::TRUE: return "TRUE";
    case TokenType::FALSE: return "FALSE";
    case TokenType::NULL_: return "NULL_";
    case TokenType::AND: return "AND";
    case TokenType::OR: return "OR";
    case TokenType::ASM: return "ASM";
    case TokenType::ERROR_KW: return "ERROR_KEYWORD";
    case TokenType::EXIT_KW: return "EXIT_KEYWORD";

    case TokenType::STRUCT: return "STRUCT";
    case TokenType::SWITCH: return "SWITCH";
    case TokenType::CASE: return "CASE";
    case TokenType::DEFAULT: return "DEFAULT";

    case TokenType::IMM: return "IMM";
    case TokenType::MUT: return "MUT";
    case TokenType::TAKE: return "TAKE";
    case TokenType::GIVE: return "GIVE";

    case TokenType::PTR: return "PTR";
    case TokenType::NEW: return "NEW";
    case TokenType::FREE: return "FREE";

    case TokenType::IDENTIFIER: return "IDENTIFIER";

    // primitive types
    case TokenType::U0: return "U0";
    case TokenType::U8: return "U8";
    case TokenType::U16: return "U16";
    case TokenType::U32: return "U32";
    case TokenType::U64: return "U64";
    case TokenType::I8: return "I8";
    case TokenType::I16: return "I16";
    case TokenType::I32: return "I32";
    case TokenType::I64: return "I64";
    case TokenType::F64: return "F64";
    case TokenType::BOOL: return "BOOL";
    case TokenType::STRING: return "STRING";

    // literals
    case TokenType::INT_LITERAL: return "INT_LITERAL";
    case TokenType::FLOAT_LITERAL: return "FLOAT_LITERAL";
    case TokenType::STRING_LITERAL: return "STRING_LITERAL";
    case TokenType::CHAR_LITERAL: return "CHAR_LITERAL";

    // syntax
    case TokenType::LPAREN: return "LPAREN";
    case TokenType::RPAREN: return "RPAREN";
    case TokenType::LBRACE: return "LBRACE";
    case TokenType::RBRACE: return "RBRACE";
    case TokenType::LBRACK: return "LBRACK";
    case TokenType::RBRACK: return "RBRACK";
    case TokenType::LANGLE: return "LANGLE";
    case TokenType::RANGLE: return "RANGLE";
    case TokenType::COMMA: return "COMMA";
    case TokenType::DOT: return "DOT";
    case TokenType::COLON: return "COLON";
    case TokenType::SEMICOLON: return "SEMICOLON";
    case TokenType::HASH: return "HASH";

    // operators
    case TokenType::BANG: return "BANG";
    case TokenType::PLUS: return "PLUS";
    case TokenType::MINUS: return "MINUS";
    case TokenType::SLASH: return "SLASH";
    case TokenType::STAR: return "STAR";
    case TokenType::EQUAL: return "EQUAL";
    case TokenType::AMPERSAND: return "AMPERSAND";
    case TokenType::MODULO: return "MODULO";

    // compound operators
    case TokenType::WALRUS: return "WALRUS";
    case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";
    case TokenType::BANG_EQUAL: return "BANG_EQUAL";
    case TokenType::LESS_EQUAL: return "LESS_EQUAL";
    case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
    case TokenType::ARROW: return "ARROW";

    case TokenType::UNKNOWN: return "UNKNOWN";
    default: return "UNHANDLED_TOKEN_TYPE";
  }
}
