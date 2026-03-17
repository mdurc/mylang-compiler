#ifndef UTIL_H

#define _assert(cond, msg)                                \
  do {                                                    \
    if (!(cond)) {                                        \
      m_logger->report(Diag::Fatal(Span{},                \
            std::string("Internal Error: ") + msg));      \
      throw std::runtime_error(msg); /* compiler crash */ \
    }                                                     \
  } while (0)

#define _assert_nolog(cond, msg)                          \
  do {                                                    \
    if (!(cond)) {                                        \
      throw std::runtime_error(msg); /* compiler crash */ \
    }                                                     \
  } while (0)

#endif // UTIL_H
