#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>

#include "util.h"
#include "driver.h"

static void usage() {
  std::cerr << "Usage: ./a.out        \n"
            << "       [ --track-memory              ]\n"
            << "       [ --target=<macos|linux>      ]\n"
            << "       [ --arch=<x86_64|aarch64>     ]\n"
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

  TargetOS target = DEFAULT_TARGET_OS;
  TargetArch arch = DEFAULT_TARGET_ARCH;
  bool track_memory = false;

  std::vector<std::string> args;
  args.push_back(argv[0]);

  // preprocess args to check for a target architecture
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--track-memory") {
      track_memory = true;
    } else if (arg.find("--target=") == 0) {
      if (arg == "--target=linux") {
        target = TargetOS::Linux;
      } else if (arg == "--target=macos") {
        target = TargetOS::MacOS;
      } else {
        std::cerr << "Unknown target: " << arg << "\n";
        return EXIT_FAILURE;
      }
    } else if (arg.find("--arch=") == 0) {
      if (arg == "--arch=aarch64") {
        arch = TargetArch::AArch64;
      } else if (arg == "--arch=x86_64") {
        arch = TargetArch::X86_64;
      } else {
        std::cerr << "Unknown architecture: " << arg << "\n";
        return EXIT_FAILURE;
      }
    } else {
      args.push_back(arg);
    }
  }

  if (args.size() < 2) { usage(); return EXIT_FAILURE; }

  std::string initial_arg = args[1];
  if (initial_arg == "--repl") {
    drive(initial_arg, "", "", target, arch, track_memory);
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
    if (!drive(arg, infile, outfile, target, arch, track_memory)) {
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}
