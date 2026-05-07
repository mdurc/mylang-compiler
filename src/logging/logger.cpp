#include "logger.h"
#include "../loader/source_loader.h"
#include <sstream>
#include <iostream>

void Logger::report(Diagnostic diag) {
  switch (diag.type) {
    case DiagType::FATAL_ERROR: m_fatals.push_back(std::move(diag)); break;
    case DiagType::ERROR: m_errors.push_back(std::move(diag)); break;
    case DiagType::WARNING: m_warnings.push_back(std::move(diag)); break;
  }
}

void Logger::clear() {
  m_errors.clear();
  m_warnings.clear();
  m_fatals.clear();
}

static void format_rich_diagnostic(std::stringstream& ss, const Diagnostic& d, const SourceLoader* loader) {
  ss << d.to_string() << "\n";
  if (loader && d.span.row > 0) {
    std::string_view line_text = loader->get_line_text(d.span.file_id, d.span.row);
    if (!line_text.empty()) {
      ss << "  " << d.span.row << " | " << line_text << "\n";
      ss << "  " << std::string(std::to_string(d.span.row).length(), ' ') << " | ";

      int spaces = (d.span.start_col > 1) ? d.span.start_col - 1 : 0;
      ss << std::string(spaces, ' ');

      int length = (d.span.len() > 0) ? d.span.len() : 1;
      ss << "^";
      if (length > 1) {
        ss << std::string(length - 1, '~');
      }
      ss << "\n";
    }
  }
}

std::string Logger::get_diagnostic_str(const SourceLoader* loader) const {
  std::stringstream ss;
  for (const auto& d : m_warnings) format_rich_diagnostic(ss, d, loader);
  for (const auto& d : m_errors)   format_rich_diagnostic(ss, d, loader);
  for (const auto& d : m_fatals)   format_rich_diagnostic(ss, d, loader);
  return ss.str();
}
