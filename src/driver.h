#ifndef SRC_DRIVER_H
#define SRC_DRIVER_H

#include <string>

bool drive(const std::string& arg, const std::string& infile,
           const std::string& outfile);

void run_repl();

#endif // SRC_DRIVER_H
