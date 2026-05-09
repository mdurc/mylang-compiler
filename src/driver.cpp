#include "driver.h"

#include <unistd.h>

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "memory/arena.h"
#include "preprocessor/preprocessor.h"
#include "checker/typechecker.h"
#include "codegen/ir/ir_printer.h"
#include "codegen/ir/ir_visitor.h"
#include "codegen/x86_64/backend.h"
#include "codegen/aarch64/backend.h"
#include "lexer/lexer.h"
#include "loader/source_loader.h"
#include "parser/parser.h"
#include "parser/symtab.h"
#include "json_export/json_exporter.h"
#include "util.h"


namespace fs = std::filesystem;
enum class TargetStage { TOKENS, AST, SYMTAB, IR, ASM, EXE, JSON };
struct CompileAbort {}; // used to stop IR/asm generation if parser/checker finds errors

struct Toolchain {
  std::string assembler, assembler_flags;
  std::string linker, linker_flags;
};

static void assemble_and_link(const std::string& asm_code, const std::string& output_exe, TargetOS target, TargetArch arch) {
  fs::path temp_dir = fs::temp_directory_path();
  std::string temp_prefix = "mycomp_" + std::to_string(getpid()) + "_" +
                            std::to_string(std::time(nullptr)) + "_";
  fs::path asm_path = temp_dir / (temp_prefix + "out.asm");
  fs::path obj_path = temp_dir / (temp_prefix + "out.o");
  fs::path exe_path = output_exe;

  std::ofstream asm_out(asm_path);
  asm_out << asm_code;
  asm_out.close();

  Toolchain tc;
  if (arch == TargetArch::X86_64) {
    if (target == TargetOS::MacOS) {
      tc.assembler = "nasm";
      tc.assembler_flags = "-f macho64";
      tc.linker = "ld";
      tc.linker_flags = " -macos_version_min 10.13 -e _start -lSystem -no_pie"
                        " -syslibroot $(xcrun --sdk macosx --show-sdk-path)";
    } else if (target == TargetOS::Linux) {
      tc.assembler = "nasm";
      tc.assembler_flags = "-f elf64";
      tc.linker = "ld";
      tc.linker_flags = "";
    }
  } else if (arch == TargetArch::AArch64) {
    if (target == TargetOS::MacOS) {
      tc.assembler = "as";
      tc.assembler_flags = "-arch arm64";
      tc.linker = "ld";
      tc.linker_flags = " -arch arm64 -e _start -lSystem"
                        " -syslibroot $(xcrun --sdk macosx --show-sdk-path)";
    } else if (target == TargetOS::Linux) {
      tc.assembler = "as";
      tc.assembler_flags = "";
      tc.linker = "ld";
      tc.linker_flags = "";
    }
  }

  std::string assemble_cmd = tc.assembler + " " + tc.assembler_flags + " " + asm_path.string() + " -o " + obj_path.string();
  std::string link_cmd = tc.linker + " " + obj_path.string() + " -o " + exe_path.string() + " " + tc.linker_flags;

  if (std::system(assemble_cmd.c_str()) != 0) {
    fs::remove(asm_path);
    fs::remove(obj_path);
    throw std::runtime_error("Assembly failed");
  }

  if (std::system(link_cmd.c_str()) != 0) {
    fs::remove(asm_path);
    fs::remove(obj_path);
    throw std::runtime_error("Linking failed");
  }

  fs::remove(asm_path);
  fs::remove(obj_path);
}

static bool run_pipeline(TargetStage stage, const std::string& infile,
                         std::ostream& out, const std::string& exe_out,
                         TargetOS target, TargetArch arch, bool track_memory) {
  Logger logger;
  SourceLoader loader(&logger);

  std::uint32_t file_id = loader.load_file(infile);
  if (file_id == 0) {
    std::cerr << logger.get_diagnostic_str(&loader);
    return false;
  }

  ArenaAllocator arena;
  Lexer lexer(&logger, file_id, target, arch);
  Preprocessor preprocessor(&logger, &loader, target, arch);
  SymTab symtab(&arena);
  Parser parser(&logger, &arena);
  TypeChecker type_checker(&logger, &arena);
  IrVisitor ir_visitor(&symtab, &logger);

  // diagnostic-driven success (no abort)
  auto check_diags = [&]() {
    if (stage == TargetStage::JSON) return; // report LSP errors later
    if (logger.num_fatals() || logger.num_errors()) throw CompileAbort();
    if (logger.num_warnings()) {
      std::cerr << logger.get_diagnostic_str(&loader);
      logger.clear_warnings();
    }
  };

  std::vector<Token> tokens;
  std::vector<AstPtr> ast;

  try {
    // lexical analysis + preprocessing
    std::string_view source = loader.get_source(file_id);
    std::vector<Token> raw_tokens = lexer.tokenize(source);
    check_diags();

    tokens = preprocessor.process(raw_tokens);
    check_diags();

    if (stage == TargetStage::TOKENS) { Token::print_tokens(tokens, out); return true; }

    // syntax analysis
    ast = parser.parse_program(&symtab, tokens);
    check_diags();

    if (stage == TargetStage::AST) { print_ast(ast, out); return true; }
    if (stage == TargetStage::SYMTAB) { symtab.print(out); return true; }

    // semantic analysis
    type_checker.check_program(&symtab, ast);
    check_diags();

    // intermediate generation
    ir_visitor.visit_all(ast);
    check_diags();
    if (stage == TargetStage::IR) { print_ir_instructions(ir_visitor.get_instructions(), out); return true; }

    // ASM generation
    std::string asm_code;
    if (arch == TargetArch::X86_64) {
      X86_64CodeGenerator backend(&logger, target, track_memory);
      asm_code = backend.generate(ir_visitor.get_instructions(), ir_visitor.is_main_defined());
    } else if (arch == TargetArch::AArch64) {
      AArch64CodeGenerator backend(&logger, target, track_memory);
      asm_code = backend.generate(ir_visitor.get_instructions(), ir_visitor.is_main_defined());
    }
    check_diags();
    if (stage == TargetStage::ASM) { out << asm_code; return true; }

    // assembly and linking
    if (stage == TargetStage::EXE) {
      assemble_and_link(asm_code, exe_out, target, arch);
      return true;
    }

    // LSP export
    if (stage == TargetStage::JSON) {
      JsonExporter exporter(&symtab, &logger, &ast);
      out << exporter.export_to_json() << "\n";
      return true;
    }
  } catch (const CompileAbort&) {
    if (stage == TargetStage::JSON) {
      JsonExporter exporter(&symtab, &logger, &ast);
      out << exporter.export_to_json() << "\n";
      return true;
    }
    std::cerr << "Compilation aborted\n" << logger.get_diagnostic_str(&loader);
    return false;

  } catch (const std::runtime_error& internal_error) {
    logger.report(Diag::Fatal(Span{}, std::string("Unhandled System Exception: ") + internal_error.what()));
    if (stage == TargetStage::JSON) {
      JsonExporter exporter(&symtab, &logger, &ast);
      out << exporter.export_to_json() << "\n";
      return true;
    }
    std::cerr << "CRITICAL ERROR\n" << logger.get_diagnostic_str(&loader);
    return false;
  } catch (const std::exception& e) {
    logger.report(Diag::Fatal(Span{}, std::string("Unhandled System Exception: ") + e.what()));
    if (stage == TargetStage::JSON) {
      JsonExporter exporter(&symtab, &logger, &ast);
      out << exporter.export_to_json() << "\n";
      return true;
    }
    std::cerr << "CRITICAL ERROR\n" << logger.get_diagnostic_str(&loader);
    return false;
  }
  return false;
}

bool drive(const std::string& arg, const std::string& infile, const std::string& outfile, TargetOS target, TargetArch arch, bool track_memory) {
  if (arg == "--repl") { run_repl(target, arch, track_memory); return true; }

  TargetStage stage;
  if (arg == "--tokens") stage = TargetStage::TOKENS;
  else if (arg == "--ast") stage = TargetStage::AST;
  else if (arg == "--symtab") stage = TargetStage::SYMTAB;
  else if (arg == "--ir") stage = TargetStage::IR;
  else if (arg == "--asm") stage = TargetStage::ASM;
  else if (arg == "--exe") stage = TargetStage::EXE;
  else if (arg == "--json") stage = TargetStage::JSON;
  else return false;

  if (stage == TargetStage::EXE) {
    return run_pipeline(stage, infile, std::cout, outfile.empty() ? "a.out": outfile, target, arch, track_memory);
  }

  if (outfile.empty()) {
    return run_pipeline(stage, infile, std::cout, "", target, arch, track_memory);
  } else {
    std::ofstream out(outfile);
    bool ret = run_pipeline(stage, infile, out, "", target, arch, track_memory);
    out.close();
    return ret;
  }
}

// == Repl implementations: ==
static std::string make_temp(const std::string& source) {
  std::string pid = std::to_string(getpid());
  std::string temp = "/tmp/repl_source_" + pid + ".tmp.sn";
  std::ofstream out(temp);
  out << source;
  out.close();
  return temp;
}

static std::string join_lines(const std::vector<std::string>& lines) {
  std::stringstream ss;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) ss << "\n";
    ss << lines[i];
  }
  return ss.str();
}

void run_repl(TargetOS target, TargetArch arch, bool track_memory) {
  std::string line;
  std::vector<std::string> lines;
  bool in_multiline = false;

  while (true) {
    std::cout << (!in_multiline ? ">>> " : "... ");

    if (!std::getline(std::cin, line)) break; // EOF

    bool auto_run = line.empty() && !lines.empty() && in_multiline;

    if (line == "exit" || line == "quit") break;

    if (line == "help") {
      std::cout << "Commands:\n"
                << "  exit, quit - Exit the REPL\n"
                << "  help       - Show this help\n"
                << "  clear      - Clear the current input\n"
                << "  tokens     - Show tokens for current input\n"
                << "  ast        - Show AST for current input\n"
                << "  ir         - Show IR for current input\n"
                << "  asm        - Show assembly for current input\n"
                << "  run        - Execute the current input\n\n"
                << "Type your code and press Enter twice to execute.\n";
      continue;
    }

    if (line == "clear") {
      lines.clear();
      in_multiline = false;
      std::cout << "Input cleared.\n";
      continue;
    }

    if (line == "tokens" || line == "ast" || line == "ir" || line == "asm") {
      if (lines.empty()) {
        std::cout << "No input to compile.\n";
        continue;
      }
      std::string temp_input = make_temp(join_lines(lines));
      drive("--" + line, temp_input, "", target, arch, track_memory);
      std::remove(temp_input.c_str());
      continue;
    }

    if (line == "run" || auto_run) {
      if (lines.empty()) {
        std::cout << "No input to execute.\n";
        continue;
      }

      std::string pid = std::to_string(getpid());
      std::string temp_exe = "/tmp/repl_temp_" + pid + ".tmp.exe";
      std::string temp_input = make_temp(join_lines(lines));

      if (drive("--exe", temp_input, temp_exe, target, arch, track_memory)) {
        int result = std::system(temp_exe.c_str());
        if (result != 0) std::cout << "Program exited with code " << result << "\n";
        std::remove(temp_exe.c_str());
      }

      std::remove(temp_input.c_str());
      if (auto_run) {
        lines.clear();
        in_multiline = false;
      }
      continue;
    }

    if (!line.empty()) {
      lines.push_back(line);
      in_multiline = true;
    }
  }
}
