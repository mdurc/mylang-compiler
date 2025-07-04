#ifndef SRC_DRIVER_H
#define SRC_DRIVER_H

#include <string>

#include "checker/typechecker.h"
#include "codegen/ir/ir_printer.h"
#include "codegen/ir/ir_visitor.h"
#include "codegen/x86_64/asm.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "parser/symtab.h"

bool drive(const std::string& arg, const std::string& input,
           const std::string& output);

bool compile_tokens(const std::string& filename, std::ostream& out);
bool compile_ast(const std::string& filename, std::ostream& out);
bool compile_symtab(const std::string& filename, std::ostream& out);
bool compile_ir(const std::string& filename, std::ostream& out);
bool compile_asm(const std::string& filename, std::ostream& out);
bool compile_exe(const std::string& filename, const std::string& out_exe);

bool compile_json(const std::string& filename, std::ostream& out);

void run_repl();

#endif // SRC_DRIVER_H
