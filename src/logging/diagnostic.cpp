#include "diagnostic.h"

#include <sstream>

std::string Diagnostic::to_string() const {
  std::stringstream ss;
  switch (type) {
    case DiagType::FATAL_ERROR: ss << "[FATAL_ERROR]"; break;
    case DiagType::ERROR: ss << "[ERROR]"; break;
    case DiagType::WARNING: ss << "[WARNING]"; break;
    default: ss << "[UNKNWON_TYPE]"; break;
  }
  if (span.row == 0 && span.len() == 0) {
    ss << ": " << msg;
  } else {
    ss << " " << span << ": " << msg;
  }
  return ss.str();
}

namespace Diag {
Diagnostic ExpectedToken(const Span& span, const std::string& expected, const std::string& got) {
  return {DiagType::ERROR, span, "Expected token '" + expected + "', got '" + got + "'"};
}

Diagnostic TypeMismatch(const Span& span, const std::string& expected, const std::string& got) {
  return {DiagType::ERROR, span, "Type mismatch: Expected '" + expected + "', got '" + got + "'"};
}

Diagnostic VariableNotFound(const Span& span, const std::string& identifier) {
  return {DiagType::ERROR, span, "Variable Not Found: identifier '" + identifier + "'"};
}

Diagnostic TypeNotFound(const Span& span, const std::string& type) {
  return {DiagType::ERROR, span, "Type Not Found: '" + type + "'"};
}

Diagnostic DuplicateDeclaration(const Span& span, const std::string& identifier) {
  return {DiagType::ERROR, span, "Duplicate Declaration: identifier '" + identifier + "'"};
}

Diagnostic Error(const Span& span, const std::string& msg) {
  return {DiagType::ERROR, span, msg};
}

Diagnostic Warning(const Span& span, const std::string& msg) {
  return {DiagType::WARNING, span, msg};
}

Diagnostic Fatal(const Span& span, const std::string& msg) {
  return {DiagType::FATAL_ERROR, span, msg};
}
} // namespace Diag
