#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <sstream>
#include <stdexcept>

enum class TargetOS {
  MacOS,
  Linux
};

enum class TargetArch {
  X86_64,
  AArch64
};

#if defined(__APPLE__) || defined(__MACH__)
    #define DEFAULT_TARGET_OS TargetOS::MacOS
#elif defined(__linux__)
    #define DEFAULT_TARGET_OS TargetOS::Linux
#else
    #define DEFAULT_TARGET_OS TargetOS::MacOS // Fallback
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
    #define DEFAULT_TARGET_ARCH TargetArch::AArch64
#elif defined(__x86_64__) || defined(_M_X64)
    #define DEFAULT_TARGET_ARCH TargetArch::X86_64
#else
    #define DEFAULT_TARGET_ARCH TargetArch::X86_64 // Fallback
#endif

#define _assert(cond, msg)                                  \
  do {                                                      \
    if (!(cond)) {                                          \
      m_logger->report(Diag::Fatal(Span{},                  \
            std::string("Internal: ") + std::string(msg))); \
      throw std::runtime_error(msg); /* compiler crash */   \
    }                                                       \
  } while (0)

#define _assert_nolog(cond, msg)                          \
  do {                                                    \
    if (!(cond)) {                                        \
      throw std::runtime_error(msg); /* compiler crash */ \
    }                                                     \
  } while (0)

inline std::string escape_string(const std::string& s) {
  std::ostringstream out;
  for (char c : s) {
    switch (c) {
      case '\n': out << "\\n"; break;
      case '\t': out << "\\t"; break;
      case '\r': out << "\\r"; break;
      case '\\': out << "\\\\"; break;
      case '"':  out << "\\\""; break;
      default:   out << c; break;
    }
  }
  return out.str();
}

#endif // UTIL_H
