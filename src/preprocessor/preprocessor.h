#ifndef PREPROCESSOR_PREPROCESSOR_H
#define PREPROCESSOR_PREPROCESSOR_H

#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../logging/logger.h"
#include "../lexer/token.h"
#include "../loader/source_loader.h"

class Preprocessor {
public:
  Preprocessor(Logger* logger, SourceLoader* loader)
    : m_logger(logger), m_loader(loader) {}

  /* takes lexed tokens and returns the macro-expanded stream */
  std::vector<Token> process(const std::vector<Token>& raw_tokens);

private:
  Logger* m_logger;
  SourceLoader* m_loader;

  /* macros map an identifier to a sequence of replacement tokens */
  std::unordered_map<std::string_view, std::vector<Token>> m_macros;

  /* track circular includes via file_id from loader */
  std::unordered_set<std::uint32_t> m_processing_files;
  std::vector<std::string> m_include_path;

  void handle_define(const std::vector<Token>& tokens, size_t& i);
  void handle_include(const std::vector<Token>& tokens, size_t& i, std::vector<Token>& output);
  std::string find_include_file(std::string_view include_name, std::uint32_t current_file_id);
};

#endif // PREPROCESSOR_PREPROCESSOR_H
