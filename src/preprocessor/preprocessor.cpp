#include "preprocessor.h"
#include "../lexer/lexer.h"
#include <filesystem>

std::vector<Token> Preprocessor::process(const std::vector<Token>& raw_tokens) {
  std::vector<Token> output;
  output.reserve(raw_tokens.size());

  for (size_t i = 0; i < raw_tokens.size(); ++i) {
    const Token& token = raw_tokens[i];
    // directives
    if (token.get_type() == TokenType::HASH) {
      if (i + 1 < raw_tokens.size() && raw_tokens[i + 1].get_type() == TokenType::IDENTIFIER) {
        std::string_view directive = raw_tokens[i + 1].get_lexeme();
        if (directive == "define") {
          handle_define(raw_tokens, i);
          continue;
        } else if (directive == "include") {
          handle_include(raw_tokens, i, output);
          continue;
        }
      }
      m_logger->report(Diag::Error(token.get_span(), "Unknown preprocessor directive"));
      continue;
    }

    // macro expansions
    if (token.get_type() == TokenType::IDENTIFIER) {
      auto it = m_macros.find(token.get_lexeme());
      if (it != m_macros.end()) {
        const std::vector<Token>& replacement = it->second;
        output.insert(output.end(), replacement.begin(), replacement.end());
        continue;
      }
    }
    // normal token, pass it through
    output.push_back(token);
  }

  return output;
}

void Preprocessor::handle_define(const std::vector<Token>& tokens, size_t& i) {
  std::uint32_t directive_row = tokens[i].get_span().row;
  i += 2;
  if (i >= tokens.size() || tokens[i].get_type() != TokenType::IDENTIFIER) {
    m_logger->report(Diag::Error(tokens[i-1].get_span(), "Expected macro name after #define"));
    return;
  }

  std::string_view macro_name = tokens[i].get_lexeme();
  i++;

  // collect all tokens that are on the exact same row
  std::vector<Token> replacement_tokens;
  while (i < tokens.size() && tokens[i].get_span().row == directive_row) {
    replacement_tokens.push_back(tokens[i]);
    i++;
  }
  m_macros[macro_name] = replacement_tokens;
  i--;
}

void Preprocessor::handle_include(const std::vector<Token>& tokens, size_t& i, std::vector<Token>& output) {
  std::uint32_t directive_row = tokens[i].get_span().row;
  i += 2;
  if (i >= tokens.size() || tokens[i].get_type() != TokenType::STRING_LITERAL) {
    m_logger->report(Diag::Error(tokens[i-1].get_span(), "Expected string literal after #include"));
    return;
  }

  std::string include_name = tokens[i].get_string_val();
  std::string full_path = find_include_file(include_name);
  if (full_path.empty()) {
    m_logger->report(Diag::Error(tokens[i].get_span(), "Could not resolve include file: " + include_name));
    return;
  }

  std::uint32_t file_id = m_loader->load_file(full_path);
  if (file_id == -1) return;

  if (m_processing_files.find(file_id) != m_processing_files.end()) {
    m_logger->report(Diag::Error(tokens[i].get_span(), "Circular include detected for file: " + include_name));
    return;
  }

  // recursively lex and preprocess the included file
  m_processing_files.insert(file_id);

  Lexer lexer(m_logger, file_id);
  std::vector<Token> raw_included_tokens = lexer.tokenize(m_loader->get_source(file_id));
  std::vector<Token> processed_included_tokens = process(raw_included_tokens);
  output.insert(output.end(), processed_included_tokens.begin(), processed_included_tokens.end());
  m_processing_files.erase(file_id);

  // consume any remaining tokens on the #include line
  while (i < tokens.size() && tokens[i].get_span().row == directive_row) {
    i++;
  }
  i--;
}

std::string Preprocessor::find_include_file(std::string_view include_name) {
  for (const std::string& path : m_include_path) {
    std::filesystem::path full_path = std::filesystem::path(path) / include_name;
    if (std::filesystem::exists(full_path) && std::filesystem::is_regular_file(full_path)) {
      return full_path.string();
    }
  }
  return "";
}
