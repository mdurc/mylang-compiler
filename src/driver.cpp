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
#include "codegen/x86_64/asm.h"
#include "lexer/lexer.h"
#include "loader/source_loader.h"
#include "parser/parser.h"
#include "parser/symtab.h"
#include "json_export/json_exporter.h"


namespace fs = std::filesystem;
enum class TargetStage { TOKENS, AST, SYMTAB, IR, ASM, EXE, JSON };
struct CompileAbort {}; // used to stop IR/asm generation if parser/checker finds errors

static void assemble_and_link(const std::string& asm_code, const std::string& output_exe) {
  fs::path temp_dir = fs::temp_directory_path();
  std::string temp_prefix = "mycomp_" + std::to_string(getpid()) + "_" +
                            std::to_string(std::time(nullptr)) + "_";
  fs::path asm_path = temp_dir / (temp_prefix + "out.asm");
  fs::path obj_path = temp_dir / (temp_prefix + "out.o");
  fs::path exe_path = output_exe;

  std::ofstream asm_out(asm_path);
  asm_out << asm_code;
  asm_out.close();

  std::string assemble_cmd = "nasm -f macho64 " + asm_path.string() + " -o " + obj_path.string();
  if (std::system(assemble_cmd.c_str()) != 0) {
    fs::remove(asm_path);
    fs::remove(obj_path);
    throw std::runtime_error("Assembly failed");
  }

  std::string link_cmd =
      "ld " + obj_path.string() + " -o " + exe_path.string() +
      " -macos_version_min 10.13 -e _start -lSystem -no_pie" +
      " -syslibroot $(xcrun --sdk macosx --show-sdk-path)";

  if (std::system(link_cmd.c_str()) != 0) {
    fs::remove(asm_path);
    fs::remove(obj_path);
    throw std::runtime_error("Linking failed");
  }

  fs::remove(asm_path);
  fs::remove(obj_path);
}

static bool run_pipeline(TargetStage stage, const std::string& input_file,
                         std::ostream& out, const std::string& exe_out = "") {
  Logger logger;
  SourceLoader loader(&logger);

  std::uint32_t file_id = loader.load_file(input_file);
  if (file_id == 0) {
    std::cerr << logger.get_diagnostic_str();
    return false;
  }

  ArenaAllocator arena;
  Lexer lexer(&logger, file_id);
  Preprocessor preprocessor(&logger, &loader);
  SymTab symtab(&arena);
  Parser parser(&logger, &arena);
  TypeChecker type_checker(&logger, &arena);
  IrVisitor ir_visitor(&symtab, &logger);
  X86_64CodeGenerator codegen(&logger);

  // diagnostic-driven success (no abort)
  auto check_diags = [&]() {
    if (stage == TargetStage::JSON) return; // report LSP errors later
    if (logger.num_fatals() || logger.num_errors()) throw CompileAbort();
    if (logger.num_warnings()) {
      std::cerr << logger.get_diagnostic_str();
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
    std::string asm_code = codegen.generate(ir_visitor.get_instructions(), ir_visitor.is_main_defined());
    check_diags();
    if (stage == TargetStage::ASM) { out << asm_code; return true; }

    // assembly and linking
    if (stage == TargetStage::EXE) {
      assemble_and_link(asm_code, exe_out);
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
    std::cerr << "Compilation aborted\n" << logger.get_diagnostic_str();
    return false;

  } catch (const std::runtime_error& internal_error) {
    if (stage == TargetStage::JSON) {
      JsonExporter exporter(&symtab, &logger, &ast);
      out << exporter.export_to_json() << "\n";
      return true;
    }
    std::cerr << "CRITICAL ERROR\n" << logger.get_diagnostic_str();
    return false;
  } catch (const std::exception& e) {
    logger.report(Diag::Fatal(Span{}, std::string("Unhandled System Exception: ") + e.what()));
    if (stage == TargetStage::JSON) {
      JsonExporter exporter(&symtab, &logger, &ast);
      out << exporter.export_to_json() << "\n";
      return true;
    }
    std::cerr << "CRITICAL ERROR\n" << logger.get_diagnostic_str();
    return false;
  }
  return false;
}

bool drive(const std::string& arg, const std::string& input, const std::string& output) {
  if (arg == "--repl") { run_repl(); return true; }

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
    if (output.empty()) throw std::runtime_error("Exe output file must exist");
    return run_pipeline(stage, input, std::cout, output);
  }

  if (output.empty()) {
    return run_pipeline(stage, input, std::cout);
  } else {
    std::ofstream out(output);
    bool ret = run_pipeline(stage, input, out);
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

void run_repl() {
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
      drive("--" + line, temp_input, "");
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

      if (drive("--exe", temp_input, temp_exe)) {
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
