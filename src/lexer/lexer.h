#ifndef LEXER_LEXER_H
#define LEXER_LEXER_H

#include <string>
#include <vector>

#include "../logging/logger.h"
#include "../preprocessor/preprocessor.h"
#include "token.h"

class Lexer {
public:
  Lexer(Logger* logger) : m_logger(logger), m_preprocessor(logger) {}

  std::vector<Token> tokenize_file(const std::string& filename);

private:
  Logger* m_logger;
  Preprocessor m_preprocessor;

  std::string m_source;
  size_t m_lex_start;
  size_t m_lex_pos;

  std::vector<Token> m_tokens;
  int m_row;
  int m_col;
  int m_start_col;

  static const std::unordered_map<std::string, TokenType> s_keyword_map;

  void setup(const std::string& source);

  std::vector<Token> tokenize();

  void scan_token();

  bool is_at_end() const;
  char advance();
  char peek() const;
  char peek_next() const;
  bool match(char expected);

  void add_token(TokenType type, Token::Lit literal_value = std::monostate{});

  char handle_escape_sequence();
  void lex_string();
  void lex_char();
  void lex_number();
  std::string read_identifier();
  void lex_identifier_or_keyword();

  void skip_whitespace();
  void skip_to_newline();
  void skip_whitespace_and_comments();
};

#endif // LEXER_LEXER_H
