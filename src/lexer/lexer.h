#ifndef LEXER_LEXER_H
#define LEXER_LEXER_H

#include <string_view>
#include <unordered_map>
#include <vector>

#include "../logging/logger.h"
#include "token.h"

class Lexer {
public:
  Lexer(Logger* logger, std::uint32_t file_id)
    : m_logger(logger), m_file_id(file_id) {}

  /* returns a tokenized version of a source code stream */
  std::vector<Token> tokenize(std::string_view source);

private:
  Logger* m_logger;
  std::uint32_t m_file_id;

  const char* m_start = nullptr;
  const char* m_pos = nullptr;
  const char* m_end = nullptr;

  std::vector<Token> m_tokens;
  std::uint32_t m_row = 1;
  std::uint16_t m_col = 1;
  std::uint16_t m_start_col = 1;

  static const std::unordered_map<std::string_view, TokenType> s_keyword_map;

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
  std::string_view read_identifier();
  void lex_identifier_or_keyword();

  void skip_whitespace();
  void skip_to_newline();
  void skip_whitespace_and_comments();
};

#endif // LEXER_LEXER_H
