#ifndef LOGGING_LOGGER_H
#define LOGGING_LOGGER_H

#include <vector>
#include <sstream>

#include "diagnostic.h"

class SourceLoader;

class Logger {
public:
  Logger() = default;

  void report(Diagnostic diag);

  const std::vector<Diagnostic>& get_errors() const { return m_errors; }
  const std::vector<Diagnostic>& get_warnings() const { return m_warnings; }
  const std::vector<Diagnostic>& get_fatals() const { return m_fatals; }

  size_t num_errors() const { return m_errors.size(); }
  size_t num_warnings() const { return m_warnings.size(); }
  size_t num_fatals() const { return m_fatals.size(); }

  void clear();
  void clear_warnings() { m_warnings.clear(); }

  std::string get_diagnostic_str(const SourceLoader* loader) const;

private:
  std::vector<Diagnostic> m_errors;
  std::vector<Diagnostic> m_fatals;
  std::vector<Diagnostic> m_warnings;
};

#endif // LOGGING_LOGGER_H
