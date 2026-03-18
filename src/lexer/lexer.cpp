#include "lexer.h"

#define _report_error(msg)                                                            \
  do {                                                                                \
    m_logger->report(Diag::Error(Span(m_file_id, m_row, m_start_col, m_col), (msg))); \
  } while (0);

const std::unordered_map<std::string_view, TokenType> Lexer::s_keyword_map = {
    {"func", TokenType::FUNC},     {"if", TokenType::IF},
    {"else", TokenType::ELSE},     {"for", TokenType::FOR},
    {"while", TokenType::WHILE},   {"read", TokenType::READ},
    {"return", TokenType::RETURN}, {"print", TokenType::PRINT},
    {"struct", TokenType::STRUCT}, {"returns", TokenType::RETURNS},
    {"case", TokenType::CASE},     {"switch", TokenType::SWITCH},
    {"break", TokenType::BREAK},   {"default", TokenType::DEFAULT},
    {"mut", TokenType::MUT},       {"continue", TokenType::CONTINUE},
    {"take", TokenType::TAKE},     {"imm", TokenType::IMM},
    {"ptr", TokenType::PTR},       {"give", TokenType::GIVE},
    {"new", TokenType::NEW},       {"free", TokenType::FREE},
    {"asm", TokenType::ASM},       {"error", TokenType::ERROR_KW},
    {"exit", TokenType::EXIT_KW},  {"true", TokenType::TRUE},
    {"false", TokenType::FALSE},   {"null", TokenType::NULL_},
    {"and", TokenType::AND},       {"or", TokenType::OR},
    {"u0", TokenType::U0},         {"u8", TokenType::U8},
    {"u16", TokenType::U16},       {"u32", TokenType::U32},
    {"u64", TokenType::U64},       {"i8", TokenType::I8},
    {"i16", TokenType::I16},       {"i32", TokenType::I32},
    {"i64", TokenType::I64},       {"f64", TokenType::F64},
    {"bool", TokenType::BOOL},     {"string", TokenType::STRING}};

std::vector<Token> Lexer::tokenize(std::string_view source) {
  m_start = source.data();
  m_pos = source.data();
  m_end = source.data() + source.size();

  m_tokens.clear();
  m_row = 1;
  m_col = 1;
  m_start_col = 1;

  while (!is_at_end()) {
    skip_whitespace_and_comments();
    if (is_at_end()) break;
    m_start_col = m_col;
    m_start = m_pos;
    scan_token();
  }

  m_start = m_pos;
  m_start_col = m_col;
  add_token(TokenType::EOF_TOK);

  return m_tokens;
}

bool Lexer::is_at_end() const { return m_pos >= m_end; }

char Lexer::advance() {
  if (is_at_end()) return '\0';
  char current_char = *m_pos++;
  if (current_char == '\n') {
    ++m_row;
    m_col = 1;
  } else {
    m_col++;
  }
  return current_char;
}

char Lexer::peek() const {
  if (is_at_end()) return '\0';
  return *m_pos;
}

char Lexer::peek_next() const {
  if (m_pos + 1 >= m_end) return '\0';
  return *(m_pos + 1);
}

bool Lexer::match(char expected) {
  if (is_at_end() || *m_pos != expected) {
    return false;
  }
  advance(); // consume the character
  return true;
}

// optional literal value
void Lexer::add_token(TokenType type, Token::Lit literal_value) {
  std::string_view lexeme(m_start, m_pos - m_start);
  m_tokens.emplace_back(type, lexeme, Span(m_file_id, m_row, m_start_col, m_col), std::move(literal_value));
}

void Lexer::skip_whitespace() {
  while (!is_at_end() && isspace(peek())) {
    advance();
  }
}

void Lexer::skip_to_newline() {
  while (!is_at_end() && peek() != '\n') {
    advance();
  }
}

void Lexer::skip_whitespace_and_comments() {
  while (true) {
    char c = peek();
    if (isspace(c)) {
      advance();
    } else if (c == '/' && peek_next() == '/') {
      // single line comments
      skip_to_newline();
    } else if (c == '/' && peek_next() == '*') {
      // block comments
      advance(); // skip '/;
      advance(); // skip '*'
      while (!is_at_end()) {
        if (peek() == '*' && peek_next() == '/') {
          advance(); // consume *
          advance(); // consume /
          break;
        }
        if (is_at_end()) break;
        advance();
      }
    } else {
      break;
    }
  }
}

char Lexer::handle_escape_sequence() {
  if (is_at_end()) {
    _report_error("Unterminated escape sequence at end of file");
    return '\0';
  }

  char esc = advance();
  switch (esc) {
    case 'n': return '\n';
    case 't': return '\t';
    case '"': return '"';
    case '\'': return '\'';
    case '\\': return '\\';
    default:
      _report_error("Unknown escape sequence: \\" + std::string(1, esc));
      return esc;
  }
}

void Lexer::lex_string() {
  // Opening '"' was consumed by scan_token calling advance()
  std::string value; // escape codes require real allocation, not view
  while (peek() != '"' && peek() != '\n' && !is_at_end()) {
    if (peek() == '\\') {
      advance();
      value += handle_escape_sequence();
    } else {
      value += advance();
    }
  }

  if (is_at_end() || peek() != '"') {
    _report_error("Unterminated string.");
    add_token(TokenType::UNKNOWN);
    return;
  }
  advance(); // Consume closing "
  add_token(TokenType::STRING_LITERAL, value);
}

void Lexer::lex_char() {
  // Opening single quote was consumed by scan_token calling advance()
  // check if it is an empty char literal, which is not allowed
  char c = peek();
  if (c == '\'') {
    _report_error("Empty char literal");
    advance(); // Consume closing '
    add_token(TokenType::UNKNOWN);
    return;
  }

  char value = '\0';
  if (c == '\\') {
    advance();
    value = handle_escape_sequence();
  } else {
    value = advance();
  }

  if (peek() != '\'') {
    _report_error("Improperly terminated char literal");
    add_token(TokenType::UNKNOWN);
    return;
  }

  advance(); // closing single quote
  add_token(TokenType::CHAR_LITERAL, static_cast<std::uint64_t>(value));
}

void Lexer::lex_number() {
  bool is_float = false;

  while (isdigit(peek())) {
    advance();
  }

  if (peek() == '.' && isdigit(peek_next())) {
    is_float = true;
    advance();
    while (isdigit(peek())) {
      advance();
    }
  }

  std::string_view num_str(m_start, m_pos - m_start);

  if (is_float) {
    try {
      add_token(TokenType::FLOAT_LITERAL, std::stod(std::string(num_str)));
    } catch (const std::out_of_range&) {
      _report_error("Float literal out of range: " + std::string(num_str));
      add_token(TokenType::UNKNOWN);
    }
  } else {
    try {
      add_token(TokenType::INT_LITERAL, std::stoull(std::string(num_str)));
    } catch (const std::out_of_range&) {
      _report_error("Integer literal out of range: " + std::string(num_str));
      add_token(TokenType::UNKNOWN);
    }
  }
}

std::string_view Lexer::read_identifier() {
  while (!is_at_end() && (isalnum(peek()) || peek() == '_')) {
    advance();
  }
  return std::string_view(m_start, m_pos - m_start);
}

void Lexer::lex_identifier_or_keyword() {
  // first char is already checked to be isalpha or '_', but not consumed
  std::string_view text = read_identifier();

  // regular keyword or identifier
  auto i = s_keyword_map.find(text);
  if (i != s_keyword_map.end()) {
    add_token(i->second);
  } else {
    add_token(TokenType::IDENTIFIER);
  }
}

void Lexer::scan_token() {
  char c = peek();

  if (isalpha(c) || c == '_') {
    lex_identifier_or_keyword();
    return;
  }

  if (isdigit(c)) {
    lex_number();
    return;
  }

  // consume the character for single/multi-char tokens
  advance();

  switch (c) {
    case '(': add_token(TokenType::LPAREN); break;
    case ')': add_token(TokenType::RPAREN); break;
    case '{': add_token(TokenType::LBRACE); break;
    case '}': add_token(TokenType::RBRACE); break;
    case '[': add_token(TokenType::LBRACK); break;
    case ']': add_token(TokenType::RBRACK); break;
    case ',': add_token(TokenType::COMMA); break;
    case '.': add_token(TokenType::DOT); break;
    case '#': add_token(TokenType::HASH); break;
    case ';': add_token(TokenType::SEMICOLON); break;
    case '+': add_token(TokenType::PLUS); break;
    case '*': add_token(TokenType::STAR); break;
    case '/': add_token(TokenType::SLASH); break;
    case '%': add_token(TokenType::MODULO); break;
    case '&': add_token(TokenType::AMPERSAND); break;

    case ':':
      add_token(match('=') ? TokenType::WALRUS : TokenType::COLON);
      break;
    case '=':
      add_token(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL);
      break;
    case '!':
      add_token(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);
      break;
    case '<':
      add_token(match('=') ? TokenType::LESS_EQUAL : TokenType::LANGLE);
      break;
    case '>':
      add_token(match('=') ? TokenType::GREATER_EQUAL : TokenType::RANGLE);
      break;
    case '-':
      add_token(match('>') ? TokenType::ARROW : TokenType::MINUS);
      break;

    case '"': lex_string(); break;
    case '\'': lex_char(); break;

    default:
      _report_error("Unexpected character: " + std::string(1, c));
      add_token(TokenType::UNKNOWN);
      break;
  }
}
