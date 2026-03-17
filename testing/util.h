#ifndef UTIL_H
#define UTIL_H

#include <string>

std::string rtrim(const std::string& s);
std::string generate_compiler_output(const std::string& flag,
                                     const std::string& input_filepath);

#endif
