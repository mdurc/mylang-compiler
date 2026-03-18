CC = g++
MYLIB = compiler_build_files
PROGRAM = $(MYLIB)/mycompiler
BUILD_DIR = $(MYLIB)/build
CFLAGS = -std=c++20 -Wall -Wextra -g -MMD -MP

# sources
LEXER_SRCS = src/lexer/lexer.cpp \
						 src/lexer/token.cpp
PREPROCESSOR_SRCS = src/preprocessor/preprocessor.cpp
DIAG_SRCS = src/logging/diagnostic.cpp
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
							src/codegen/x86_64/asm.cpp
LSP_SRC = src/json_export/json_exporter.cpp

# all files
SOURCE = $(LEXER_SRCS) $(PREPROCESSOR_SRCS) $(DIAG_SRCS) \
         $(LOADER_SRCS) $(MEMORY_SRCS) $(PARSER_SRC) \
         $(CHECKER_SRC) $(CODEGEN_SRC) $(LSP_SRC)

PROGRAM_SRCS = src/driver.cpp src/main.cpp $(SOURCE)
PROGRAM_OBJS = $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(PROGRAM_SRCS))

RUNTIME_ASM = src/codegen/runtime/x86_64_lib.asm
RUNTIME_OBJ = $(MYLIB)/runtime.o

# rules
all: $(PROGRAM)

$(PROGRAM): $(PROGRAM_OBJS)
	mkdir -p $(MYLIB)
	$(CC) $(CFLAGS) $(PROGRAM_OBJS) -o $@

$(BUILD_DIR)/%.o: src/%.cpp
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# test workflow
TFILE = test.sn
update_test: $(PROGRAM)
	mkdir -p $(BUILD_DIR)
	./$(PROGRAM) \
	--tokens $(TFILE) $(TFILE).tokens \
	--ast $(TFILE) $(TFILE).ast \
	--symtab $(TFILE) $(TFILE).symtab \
	--ir $(TFILE) $(TFILE).ir \
	--asm $(TFILE) $(TFILE).asm \
	--exe $(TFILE) $(TFILE).exe

compile_test_asm:
	nasm -f macho64 $(TFILE).asm -o $(BUILD_DIR)/test_main.o
	ld $(BUILD_DIR)/test_main.o -o $(TFILE).exe \
	-macos_version_min 10.13 -e _start -lSystem -no_pie \
	-syslibroot $(shell xcrun --sdk macosx --show-sdk-path)

test: update_test compile_test_asm
	./$(TFILE).exe

clean:
	rm -rf $(MYLIB)
	rm -f $(TFILE).* a.out

.PHONY: all clean update_test compile_test_asm test
-include $(PROGRAM_OBJS:.o=.d)
