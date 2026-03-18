#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_set>

#include "driver.h"

static void usage() {
  std::cerr << "Usage: ./a.out        \n"
            << "       [ --tokens <infile> <outfile> ]\n"
            << "       [ --ast    <infile> <outfile> ]\n"
            << "       [ --symtab <infile> <outfile> ]\n"
            << "       [ --ir     <infile> <outfile> ]\n"
            << "       [ --asm    <infile> <outfile> ]\n"
            << "       [ --exe    <infile> <outfile> ]\n"
            << "       [ --repl ]\n";
}

int main(int argc, char** argv) {
  if (argc < 2) {
    usage();
    return EXIT_FAILURE;
  }

  std::string initial_arg = argv[1];
  if (initial_arg == "--repl") {
    drive(initial_arg, "", "");
    return EXIT_SUCCESS;
  }
  std::unordered_set<std::string> opts = {"--tokens", "--ast", "--symtab",
                                          "--ir",     "--asm", "--exe",
                                          "--json",   "--repl"};
  for (int i = 1; i < argc;) {
    std::string arg = argv[i];

    if (opts.find(arg) == opts.end()) {
      std::cerr << "Unknown option: " << arg << "\n";
      return EXIT_FAILURE;
    }

    std::string infile = "", outfile = "";
    if (i + 1 < argc && argv[i + 1][0] != '-') {
      infile = argv[i + 1];
      i += 1;
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        outfile = argv[i + 1];
        i += 1;
      }
      i += 1;
    } else {
      std::cerr << "Must provide an infile after arg: " << arg << "\n";
      break;
    }

    // std::cerr << "arg: " << arg << ", to " << infile << " -> " << outfile << "\n";
    if (!drive(arg, infile, outfile)) {
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}
