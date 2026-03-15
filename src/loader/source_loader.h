#ifndef SOURCE_LOADER_H
#define SOURCE_LOADER_H

#include <cstdint>
#include "../logging/logger.h"
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct SourceFile {
  std::uint32_t id;
  std::string filepath;
  std::string content;
  std::vector<size_t> line_offsets;
};

class SourceLoader {
public:
  SourceLoader(Logger* logger) : m_logger(logger) {}

  /* loads a file from disk;
     returns a file id of (possibly already created) file otherwise 0 */
  std::uint32_t load_file(const std::string& filepath);

  /* lexer will retrieve file source contents */
  std::string_view get_source(std::uint32_t file_id) const;
  std::string_view get_filepath(std::uint32_t file_id) const;

  /* logger can retrieve the raw text content for a specific line (1-indexed) */
  std::string_view get_line_text(std::uint32_t file_id, std::uint32_t line) const;

private:
  Logger* m_logger;

  /* unique ptr is used so that the lexer's ptrs
     won't be invalidated upon m_files reallocation */
  std::vector<std::unique_ptr<SourceFile>> m_files;

  /* stop redundant loading of files */
  std::unordered_map<std::string, std::uint32_t> m_loaded_paths;

  /* precomputes where lines start for the logger */
  void compute_line_offsets(SourceFile* file);
};

#endif // SOURCE_LOADER_H
