#ifndef LEXER_SPAN_H
#define LEXER_SPAN_H

#include <cstddef>
#include <ostream>

/* stored within each token as the location within the raw text */
struct Span {
  std::uint32_t file_id;
  std::uint32_t row;
  std::uint16_t start_col;
  std::uint16_t end_col; // exclusive

  Span() : file_id(-1), row(0), start_col(0), end_col(0) {}
  Span(std::uint32_t file_id, std::uint32_t row, std::uint16_t start_col, std::uint16_t end_col)
      : file_id(file_id), row(row), start_col(start_col), end_col(end_col) {}

  inline std::uint32_t len() const { return end_col - start_col; }

  inline std::string to_string() const {
    return "(file=" + std::to_string(file_id) +
      ", row=" + std::to_string(row) +
      ", col=" + std::to_string(start_col) + "-" +
      std::to_string(end_col) + ")";
  }

  inline friend std::ostream& operator<<(std::ostream& out, const Span& span) {
    out << span.to_string();
    return out;
  }

  inline bool operator>(const Span& other) const {
    if (file_id != other.file_id) return file_id > other.file_id;
    if (row != other.row) return row > other.row;
    if (start_col != other.start_col) return start_col > other.start_col;
    return end_col > other.end_col;
  }
};

#endif // LEXER_SPAN_H
