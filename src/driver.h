#ifndef SRC_DRIVER_H
#define SRC_DRIVER_H

#include "codegen/x86_64/backend.h"
#include "util.h"
#include <string>

bool drive(const std::string& arg, const std::string& infile,
           const std::string& outfile, TargetOS target);

void run_repl(TargetOS target);

#endif // SRC_DRIVER_H
