#include <map>
#include <sstream>
#include <iomanip>
#include <type_traits>
#include <string_view>

namespace {
class json {
public:
  enum class Type { Null, Object, Array, String, Number, Bool };
  Type type = Type::Null;

  std::map<std::string, json> object_vals;
  std::vector<json> array_vals;
  std::string string_val;
  double number_val = 0;
  bool bool_val = false;

  json() = default;
  json(const char *s) : type(Type::String), string_val(s) {}
  json(const std::string &s) : type(Type::String), string_val(s) {}
  json(bool b) : type(Type::Bool), bool_val(b) {}

  // any integer type (int, uint64_t, size_t, long long)
  template <typename T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value, int>::type = 0>
  json(T n) : type(Type::Number), number_val(static_cast<double>(n)) {}

  // any floating point type (float, double)
  template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
  json(T n) : type(Type::Number), number_val(static_cast<double>(n)) {}

  // assignment operator overloads
  json& operator=(const char* s) { type = Type::String; string_val = s; return *this; }
  json& operator=(const std::string& s) { type = Type::String; string_val = s; return *this; }
  json& operator=(std::string_view s) { type = Type::String; string_val = s; return *this; }
  json& operator=(bool b) { type = Type::Bool; bool_val = b; return *this; }
  template <typename T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value, int>::type = 0>
  json& operator=(T n) { type = Type::Number; number_val = static_cast<double>(n); return *this; }
  template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
  json& operator=(T n) { type = Type::Number; number_val = static_cast<double>(n); return *this; }

  static json array() {
    json n;
    n.type = Type::Array;
    return n;
  }

  static json object() {
    json n;
    n.type = Type::Object;
    return n;
  }

  json &operator[](const std::string &key) {
    if (type == Type::Null)
      type = Type::Object;
    return object_vals[key];
  }

  void push_back(const json &node) {
    if (type == Type::Null)
      type = Type::Array;
    array_vals.push_back(node);
  }

  std::string dump(int indent = 0, int current_indent = 0) const {
    std::ostringstream oss;
    std::string ind(current_indent, ' ');
    std::string ind_plus(current_indent + indent, ' ');
    std::string newline = indent > 0 ? "\n" : "";
    std::string space = indent > 0 ? " " : "";

    switch (type) {
    case Type::Null:
      return "null";
    case Type::Bool:
      return bool_val ? "true" : "false";
    case Type::Number: {
      if (number_val == static_cast<long long>(number_val)) {
        oss << static_cast<long long>(number_val);
      } else {
        oss << number_val;
      }
      return oss.str();
    }
    case Type::String: {
      oss << "\"";
      for (char c : string_val) {
        switch (c) {
        case '\"':
          oss << "\\\"";
          break;
        case '\\':
          oss << "\\\\";
          break;
        case '\b':
          oss << "\\b";
          break;
        case '\f':
          oss << "\\f";
          break;
        case '\n':
          oss << "\\n";
          break;
        case '\r':
          oss << "\\r";
          break;
        case '\t':
          oss << "\\t";
          break;
        default:
          if ('\x00' <= c && c <= '\x1f') {
            oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
          } else {
            oss << c;
          }
        }
      }
      oss << "\"";
      return oss.str();
    }
    case Type::Array: {
      if (array_vals.empty())
        return "[]";
      oss << "[" << newline;
      for (size_t i = 0; i < array_vals.size(); ++i) {
        oss << ind_plus << array_vals[i].dump(indent, current_indent + indent);
        if (i < array_vals.size() - 1)
          oss << ",";
        oss << newline;
      }
      oss << ind << "]";
      return oss.str();
    }
    case Type::Object: {
      if (object_vals.empty())
        return "{}";
      oss << "{" << newline;
      size_t i = 0;
      for (auto const &[k, v] : object_vals) {
        oss << ind_plus << "\"" << k << "\":" << space
            << v.dump(indent, current_indent + indent);
        if (i < object_vals.size() - 1)
          oss << ",";
        oss << newline;
        i++;
      }
      oss << ind << "}";
      return oss.str();
    }
    }
    return "null";
  }
};
} // namespace
