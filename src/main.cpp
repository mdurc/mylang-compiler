#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>

#include "util.h"
#include "driver.h"

static void usage() {
  std::cerr << "Usage: ./a.out        \n"
            << "       [ --tokens <infile> <outfile> ]\n"
            << "       [ --ast    <infile> <outfile> ]\n"
            << "       [ --symtab <infile> <outfile> ]\n"
            << "       [ --ir     <infile> <outfile> ]\n"
            << "       [ --asm    <infile> <outfile> ]\n"
            << "       [ --exe    <infile> <outfile> ]\n"
            << "       [ --json   <infile> <outfile> ]\n"
            << "       [ --repl ] (MacOS only)\n";
}

int main(int argc, char** argv) {
  if (argc < 2) { usage(); return EXIT_FAILURE; }

  TargetOS target = TargetOS::MacOS; // default
  std::vector<std::string> args;
  args.push_back(argv[0]);

  // preprocess args to check for a target architecture
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg.find("--target=") == 0) {
      if (arg == "--target=linux") {
        target = TargetOS::Linux;
      } else if (arg == "--target=macos") {
        target = TargetOS::MacOS;
      } else {
        std::cerr << "Unknown target: " << arg << "\n";
        return EXIT_FAILURE;
      }
    } else {
      args.push_back(arg);
    }
  }

  if (args.size() < 2) { usage(); return EXIT_FAILURE; }

  std::string initial_arg = args[1];
  if (initial_arg == "--repl") {
    drive(initial_arg, "", "", target);
    return EXIT_SUCCESS;
  }

  std::unordered_set<std::string> opts = {"--tokens", "--ast", "--symtab",
                                          "--ir",     "--asm", "--exe",
                                          "--json",   "--repl"};
  for (size_t i = 1; i < args.size();) {
    std::string arg = args[i];

    if (opts.find(arg) == opts.end()) {
      std::cerr << "Unknown option: " << arg << "\n";
      return EXIT_FAILURE;
    }

    std::string infile = "", outfile = "";
    if (i + 1 < args.size() && args[i + 1][0] != '-') {
      infile = args[i + 1];
      i += 1;
      if (i + 1 < args.size() && args[i + 1][0] != '-') {
        outfile = args[i + 1];
        i += 1;
      }
      i += 1;
    } else {
      std::cerr << "Must provide an infile after arg: " << arg << "\n";
      break;
    }

    // std::cerr << "arg: " << arg << ", to " << infile << " -> " << outfile << "\n";
    if (!drive(arg, infile, outfile, target)) {
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}
