#ifndef LOGGING_DIAGNOSTIC_H
#define LOGGING_DIAGNOSTIC_H

#include <string>

#include "../lexer/span.h"
#include "../lexer/token.h"

enum class DiagType { FATAL_ERROR, ERROR, WARNING };

/* redesigned diagnostics to be a struct, more lightweight
    and it never really needed to be a subclass of std::exception */
struct Diagnostic {
  DiagType type;
  Span span;
  std::string msg;

  /* on-demand conversion to string */
  std::string to_string() const;
};

/* factory for diagnostic structs */
namespace Diag {
  Diagnostic ExpectedToken(const Span& span, const std::string& expected, const std::string& got);
  Diagnostic TypeMismatch(const Span& span, const std::string& expected, const std::string& got);
  Diagnostic VariableNotFound(const Span& span, const std::string& identifier);
  Diagnostic TypeNotFound(const Span& span, const std::string& type);
  Diagnostic DuplicateDeclaration(const Span& span, const std::string& identifier);

  // generic diagnostics
  Diagnostic Error(const Span& span, const std::string& msg);
  Diagnostic Warning(const Span& span, const std::string& msg);
  Diagnostic Fatal(const Span& span, const std::string& msg);
}

#endif // LOGGING_DIAGNOSTIC_H
