CC = clang++
MYLIB = compiler_build_files
BINARY = mycompiler
PROGRAM = $(MYLIB)/$(BINARY)
BUILD_DIR = $(MYLIB)/build
CFLAGS = -std=c++20 -Wall -Wextra -g -MMD -MP

# generating the ASM string literal that is pasted as the runtime lib
RUNTIME_ASM = src/codegen/x86_64/runtime.asm
RUNTIME_RAW = src/codegen/x86_64/runtime_raw.h

# sources
LEXER_SRCS = src/lexer/lexer.cpp \
						 src/lexer/token.cpp
PREPROCESSOR_SRCS = src/preprocessor/preprocessor.cpp
DIAG_SRCS = src/logging/diagnostic.cpp \
						src/logging/logger.cpp
LOADER_SRCS = src/loader/source_loader.cpp
MEMORY_SRCS = src/memory/arena.cpp
PARSER_SRC = src/parser/parser.cpp \
						 src/parser/symtab.cpp \
						 src/parser/ast.cpp \
						 src/parser/visitor.cpp \
						 src/parser/types.cpp
CHECKER_SRC = src/checker/typechecker.cpp
CODEGEN_SRC = src/codegen/ir/ir_generator.cpp \
							src/codegen/ir/ir_printer.cpp \
							src/codegen/ir/ir_visitor.cpp \
							src/codegen/x86_64/backend.cpp
LSP_SRC = src/json_export/json_exporter.cpp

# all files
SOURCE = $(LEXER_SRCS) $(PREPROCESSOR_SRCS) $(DIAG_SRCS) \
         $(LOADER_SRCS) $(MEMORY_SRCS) $(PARSER_SRC) \
         $(CHECKER_SRC) $(CODEGEN_SRC) $(LSP_SRC)

PROGRAM_SRCS = src/driver.cpp src/main.cpp $(SOURCE)
PROGRAM_OBJS = $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(PROGRAM_SRCS))

# rules
all: $(PROGRAM)

$(RUNTIME_RAW): $(RUNTIME_ASM)
	@echo "Generating runtime_raw.h from runtime.asm..."
	@printf '#pragma once\n#include <string>\nconst std::string RUNTIME_ASM = R"(' > $@
	@cat $< >> $@
	@printf ')";\n' >> $@

$(PROGRAM): $(PROGRAM_OBJS)
	mkdir -p $(MYLIB)
	$(CC) $(CFLAGS) $(PROGRAM_OBJS) -o $@

$(BUILD_DIR)/%.o: src/%.cpp $(RUNTIME_RAW)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

install: $(PROGRAM)
	mkdir -p ~/.local/bin
	cp $(PROGRAM) ~/.local/bin/$(BINARY)

uninstall:
	rm -f ~/.local/bin/$(BINARY)

# test workflow
TFILE = tfile.sn
update_test: $(PROGRAM)
	mkdir -p $(BUILD_DIR)
	./$(PROGRAM) \
	--tokens $(TFILE) $(TFILE).tokens \
	--ast $(TFILE) $(TFILE).ast \
	--symtab $(TFILE) $(TFILE).symtab \
	--ir $(TFILE) $(TFILE).ir \
	--asm $(TFILE) $(TFILE).asm \
	--json $(TFILE) $(TFILE).json \
	--exe $(TFILE) $(TFILE).exe

compile_test_macos:
	nasm -f macho64 $(TFILE).asm -o $(BUILD_DIR)/test_main.o
	ld $(BUILD_DIR)/test_main.o -o $(TFILE).exe \
	-macos_version_min 10.13 -e _start -lSystem -no_pie \
	-syslibroot $(shell xcrun --sdk macosx --show-sdk-path)

compile_test_linux:
	nasm -f elf64 $(TFILE).asm -o $(BUILD_DIR)/test_main.o
	ld.lld -flavor gnu $(BUILD_DIR)/test_main.o -o $(TFILE).exe

test: update_test compile_test_macos
	./$(TFILE).exe

test_linux: update_test compile_test_linux
	./$(TFILE).exe

clean:
	rm -rf $(MYLIB)
	rm -f $(TFILE).* a.out DATA.txt .DS_Store

.PHONY: all clean update_test compile_test_macos compile_test_linux test test_linux install uninstall
-include $(PROGRAM_OBJS:.o=.d)
