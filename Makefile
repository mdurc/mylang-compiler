CC = clang++
MYLIB = compiler_build_files
BINARY = mycompiler
PROGRAM = $(MYLIB)/$(BINARY)
BUILD_DIR = $(MYLIB)/build
CFLAGS = -std=c++20 -Wall -Wextra -g -MMD -MP

# OS detection for fallback within the testing rules
HOST_OS := $(if $(filter Linux,$(shell uname -s)),linux,macos)
HOST_ARCH := $(if $(filter x86_64,$(shell uname -m)),x86_64,aarch64)
TARGET_OS ?= $(HOST_OS)
TARGET_ARCH ?= $(HOST_ARCH)

# generating the ASM string literal that is pasted as the runtime lib
ifeq ($(TARGET_ARCH),aarch64)
RUNTIME_ASM = src/codegen/aarch64/runtime.s
RUNTIME_RAW = src/codegen/aarch64/runtime_raw.h
else
RUNTIME_ASM = src/codegen/x86_64/runtime.asm
RUNTIME_RAW = src/codegen/x86_64/runtime_raw.h
endif

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
							src/codegen/x86_64/backend.cpp \
							src/codegen/aarch64/backend.cpp
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
	@echo "Generating $(RUNTIME_RAW) from $(RUNTIME_ASM)..."
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

# --- test workflow -------
TFILE = tfile.sn

update_test: $(PROGRAM)
	mkdir -p $(BUILD_DIR)
	./$(PROGRAM) \
	--track-memory \
	--target=$(TARGET_OS) \
	--arch=$(TARGET_ARCH) \
	--tokens $(TFILE) $(TFILE).tokens \
	--ast $(TFILE) $(TFILE).ast \
	--symtab $(TFILE) $(TFILE).symtab \
	--ir $(TFILE) $(TFILE).ir \
	--asm $(TFILE) $(TFILE).asm \
	--json $(TFILE) $(TFILE).json \
	--exe $(TFILE) $(TFILE).exe

test: update_test
	- ./$(TFILE).exe

test-x86:
	$(MAKE) test TARGET_ARCH=x86_64

test-arm:
	$(MAKE) test TARGET_ARCH=aarch64

test_leaks: $(PROGRAM)
	leaks --atExit -- compiler_build_files/mycompiler --arch=x86_64 --tokens tfile.sn tfile.sn.tokens --ast tfile.sn tfile.sn.ast --symtab tfile.sn tfile.sn.symtab --ir tfile.sn tfile.sn.ir --asm tfile.sn tfile.sn.asm --json tfile.sn tfile.sn.json --exe tfile.sn tfile.sn.exe > LEAKS.txt 2>&1

test_valgrind: $(PROGRAM)
	valgrind ./compiler_build_files/mycompiler --arch=x86_64 --tokens tfile.sn tfile.sn.tokens --ast tfile.sn tfile.sn.ast --symtab tfile.sn tfile.sn.symtab --ir tfile.sn tfile.sn.ir --asm tfile.sn tfile.sn.asm --json tfile.sn tfile.sn.json --exe tfile.sn tfile.sn.exe > LEAKS.txt 2>&1
# -------------------------

# generate tfile.sn.asm and manually assemble/link to binary
compile_x86_macos: update_test
	nasm -f macho64 $(TFILE).asm -o $(BUILD_DIR)/test_main.o
	ld $(BUILD_DIR)/test_main.o -o $(TFILE).exe \
	-macos_version_min 10.13 -e _start -lSystem -no_pie \
	-syslibroot $(shell xcrun --sdk macosx --show-sdk-path)

compile_x86_linux: update_test
	nasm -f elf64 $(TFILE).asm -o $(BUILD_DIR)/test_main.o
	ld $(BUILD_DIR)/test_main.o -o $(TFILE).exe

compile_arm_macos: update_test
	as -arch arm64 $(TFILE).asm -o $(BUILD_DIR)/test_main.o
	ld $(BUILD_DIR)/test_main.o -o $(TFILE).exe \
	-arch arm64 -e _start -lSystem \
	-syslibroot $(shell xcrun --sdk macosx --show-sdk-path)

compile_arm_linux: update_test
	as $(TFILE).asm -o $(BUILD_DIR)/test_main.o
	ld $(BUILD_DIR)/test_main.o -o $(TFILE).exe

clean:
	rm -rf $(MYLIB)
	rm -f $(TFILE).* a.out .DS_Store DATA.txt LEAKS.txt arm_out.txt x86_out.txt

.PHONY: all clean \
	update_test \
	test test-x86 test-arm test_leaks \
	install uninstall
-include $(PROGRAM_OBJS:.o=.d)
