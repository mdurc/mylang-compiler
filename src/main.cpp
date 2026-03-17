#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_set>

#include "driver.h"

static void usage() {
  std::cerr << "Usage: ./a.out <input>\n"
            << "       [ --tokens <filename> ]\n"
            << "       [ --ast    <filename> ]\n"
            << "       [ --symtab <filename> ]\n"
            << "       [ --ir     <filename> ]\n"
            << "       [ --asm    <filename> ]\n"
            << "       [ --exe    <filename> ]\n"
            << "       [ --repl              ]\n";
}

int main(int argc, char** argv) {
  if (argc < 2) {
    usage();
    return EXIT_FAILURE;
  }

  std::string input = argv[1];
  if (input == "--repl") {
    drive(input, "", "");
    return EXIT_SUCCESS;
  }
  std::unordered_set<std::string> opts = {"--tokens", "--ast", "--symtab",
                                          "--ir",     "--asm", "--exe",
                                          "--json",   "--repl"};
  for (int i = 2; i < argc;) {
    std::string arg = argv[i];

    if (opts.find(arg) == opts.end()) {
      std::cerr << "Unknown option: " << arg << "\n";
      return EXIT_FAILURE;
    }

    std::string output = "";
    if (i + 1 < argc && argv[i + 1][0] != '-') {
      output = argv[i + 1];
      i += 2;
    } else {
      i += 1;
    }

    if (!drive(arg, input, output)) {
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}
