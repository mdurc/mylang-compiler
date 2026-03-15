#ifndef LOGGING_LOGGER_H
#define LOGGING_LOGGER_H

#include <vector>
#include <sstream>

#include "diagnostic.h"

class Logger {
public:
  Logger() = default;

  void report(Diagnostic diag) {
    switch (diag.type) {
      case DiagType::FATAL_ERROR: m_fatals.push_back(std::move(diag)); break;
      case DiagType::ERROR: m_errors.push_back(std::move(diag)); break;
      case DiagType::WARNING: m_warnings.push_back(std::move(diag)); break;
    }
  }

  const std::vector<Diagnostic>& get_errors() const { return m_errors; }
  const std::vector<Diagnostic>& get_warnings() const { return m_warnings; }
  const std::vector<Diagnostic>& get_fatals() const { return m_fatals; }

  size_t num_errors() const { return m_errors.size(); }
  size_t num_warnings() const { return m_warnings.size(); }
  size_t num_fatals() const { return m_fatals.size(); }

  void clear() {
    m_errors.clear();
    m_warnings.clear();
    m_fatals.clear();
  }

  void clear_warnings() { m_warnings.clear(); }

  std::string get_diagnostic_str() const {
    std::stringstream ss;
    for (const auto& d : m_warnings) ss << d.to_string() << "\n";
    for (const auto& d : m_errors)   ss << d.to_string() << "\n";
    for (const auto& d : m_fatals)   ss << d.to_string() << "\n";
    return ss.str();
  }

private:
  std::vector<Diagnostic> m_errors;
  std::vector<Diagnostic> m_fatals;
  std::vector<Diagnostic> m_warnings;
};

#endif // LOGGING_LOGGER_H
