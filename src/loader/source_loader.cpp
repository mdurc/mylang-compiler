#include "source_loader.h"

#include <filesystem>
#include <fstream>
#include <sstream>

std::uint32_t SourceLoader::load_file(const std::string& filepath) {
  // canonical path so that redundant elements are removed and circular inclusion works properly
  std::error_code ec;
  std::filesystem::path canonical_path = std::filesystem::canonical(filepath, ec);
  if (ec) {
    // does not exist, or we don't have permissions
    m_logger->report(Diag::Error(Span{}, "Could not resolve path: " + filepath));
    return -1;
  }

  std::string absolute_path = canonical_path.string();

  auto it = m_loaded_paths.find(absolute_path);
  if (it != m_loaded_paths.end()) return it->second;

  std::ifstream file(absolute_path);
  if (!file.is_open()) {
    m_logger->report(Diag::Error(Span{}, "Failed to open file: " + absolute_path));
    return 0;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  std::uint32_t new_id = m_files.size() + 1;

  auto source_file = std::make_unique<SourceFile>();
  source_file->id = new_id;
  source_file->filepath = absolute_path;
  source_file->content = buffer.str();

  compute_line_offsets(source_file.get());

  m_files.push_back(std::move(source_file));
  m_loaded_paths[absolute_path] = new_id;

  return new_id;
}

void SourceLoader::compute_line_offsets(SourceFile* file) {
  // line 1 starts at index 0
  file->line_offsets.push_back(0);
  for (size_t i = 0; i < file->content.size(); ++i) {
    if (file->content[i] == '\n') {
      file->line_offsets.push_back(i + 1);
    }
  }
}

std::string_view SourceLoader::get_source(std::uint32_t file_id) const {
  if (file_id == 0 || file_id > m_files.size()) return "";
  return m_files[file_id - 1]->content;
}

std::string_view SourceLoader::get_filepath(std::uint32_t file_id) const {
  if (file_id == 0 || file_id > m_files.size()) return "";
  return m_files[file_id - 1]->filepath;
}

std::string_view SourceLoader::get_line_text(std::uint32_t file_id, std::uint32_t line) const {
  if (file_id == 0 || file_id > m_files.size() || line == 0) return "";

  const std::unique_ptr<SourceFile>& file = m_files[file_id - 1];
  if (line > file->line_offsets.size()) return "";

  size_t start = file->line_offsets[line - 1];
  size_t end = (line < file->line_offsets.size()) ? file->line_offsets[line]
                                                 : file->content.size();
  // strip trailing newline character for logger
  if (end > start && file->content[end - 1] == '\n') {
    end--;
  }
  return std::string_view(file->content.data() + start, end - start);
}
