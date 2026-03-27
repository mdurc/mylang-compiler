## Architecture

The compiler is organized into a single pass pipeline:
`Source` -> `Preprocessor` -> `Lexer` -> `Parser` -> `Type Checker` -> `IR Generator` -> `x86_64 Code Generator`

I'll first try to cover some of the approaches that I was thinking about while developing. A lot of these ideas are new to me, and were added to this project recently in a large refactor of the codebase (after ~8 month break).

### Error Handling: Panic Mode & Diagnostics
The compiler is only as useful as what it is able to compile, and what it is able to express to the user regarding errors and other information. A poor error handling system disregards an entire part of a compiler project. It also demonstrates the understanding that your compiler has over the language it is compiling. If you lack well-formed error messages, it's impossible to be sure if the user made a mistake, or if the compiler has a bug.

- The current state of error handling and messages is _okay_. The general case will produce well-formed error messages, though there are some cases where (particularly when working with structs/function identifiers) the error messages are not the most descriptive of the precise error.

**Parser**
- The parser is the first stage where significant error handling design has to be decided. The main approach is to report as many actionable errors as possible in a single pass, _without cascading false positives_.
- This seems to be a common approach, called "Panic Mode Recovery".
- The diagnostics module includes different severities that can be logged, and the parser is in charge of logging an error (including the row and column in the source code that this error was found) and then entering a panic mode.
- The parser throws an exception to enter panic mode, which is eventually caught at a safe point, where the parser moves to a safe location to proceed.

**Typechecker**
- The typechecker is the second stage of this error handling system
- At this point, the typechecker is also concerned about providing type-errors, but also doesn't want to cascade redundant messages that we already reported from the parsing phase.
- This is where the poisoned branches design comes in. When the parser logs an error, it inserts a "poisoned" ast node, which the typechecker can identify and act accordingly.
- When the checker finds a poisoned node, it evaluates the type of the current expression to an `ErrorType`. This error type is then propogated up the decorated ast/type tree (skipping checks and error logs) until we start evaluating a safe branch again.
- After the typechecker, if we logged any errors, it isn't worth-while to continue with IR or ASM generation, so we stop here. However, it is important to note that we still want to perform "internal" compiler assertions, even though they shouldn't ever make it to the logger or user.

### Memory Management: Arena Allocation
This wasn't a part of the original design, although it makes a lot of sense in this use-case. Our AST was entirely composed of shared pointers to each node representing the components of a program. This seems to be terribly inefficient, although it's possible that there are some advantages with this design. It seems like a safer option to have one location where the memory that should persist across modules can live.
- This speeds up each allocation and the full deallocation at the end of the program.
- It also revealed an additional advantage: storing strings in the memory chunks allows for easy copying with `std::string_view` instead of copying by value in various locations.


## Pipeline

Follows a standard flow of passes until producing the assembly:
1. **Source loader**: this is in charge of loading source files and tracking them with a `file_id` so that we don't perform redundant loads.
2. **Lexer**: converts the source text that we loadded into a stream of tokens.
3. **Preprocessor**: this walks through the tokens and performs macro substitution on tokens for `#define` directives, and re-runs the lexer for any other `#include`d files, to insert the tokens into the final token stream.
    - File paths are relative for inclusion, see `sample_code/modules`
4. **Parser**: Builds an Abstract Syntax Tree (AST) from the token stream and populates the symbol table with initial declarations of types and variables.
5. **Type checker**: Traverses the AST to enforce type rules, resolve expression types, and check for semantic errors such as VariableNotFound.
6. **IR Generation**: Translates the annotated AST into a three-address code IR format for the code generator.
7. **Code Generation**: Converts the IR into target-specific assembly code. Utilizes a custom runtime asm library for more complex/lengthy operations such as `string_copy` or `parse_int`. This runtime library is embedded at the bottom of each assembly file that is generated (note that this is not efficient because we may not utilize all of the procedures).
8. **Assembler**: Uses external tools to assemble the generated code.

## Type Casting
- This is implemented but not tested extensively. It is an area that can undergo a lot of improvement in the future.
- As of now, we can do conversions between integer sizes.
- The approach starts in the type checker, where operations such as assignment, comparators, etc, are able to identify a possible need for an implicit type conversion. Note that we only are supporting implicit conversions as of now.
- When the type checker can perform the implicit cast, it will insert an implicit cast node that will describe what the size of the new expression should be casted to.
- In the IR generator, when we visit an implicit cast node, we will instruct the assembly generator to use the proper sizes during the operation.

## Code Generation (x86-64)
-   **Register Allocation**:
    -   Greedy selection of registers, spilling to the stack if necessary. A pool of general-purpose registers (`r10`, `r11`, `rbx`, etc) is reserved for mapping to IR temporary registers.
    -   **Spilling**: If all physical registers are in use, the code generator "spills" an existing register's value to the stack to free it up. It tracks spilled locations and reloads them when needed.
-   **Stack Frame Management**:
    -   Each function has a standard stack frame set up with `rbp` as the frame pointer.
    -   The function prologue saves the previous `rbp`, sets the new `rbp`, and subtracts from `rsp` to allocate space for all local variables, parameters passed on the stack, and spilled registers.
    -   The function epilogue restores `rsp` and `rbp` before returning.
-   **Calling Convention**:
    -   The first six integer/pointer arguments are passed in `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`. Subsequent arguments are pushed onto the stack in reverse order.
    -   The return value is placed in `rax`.
    -   The generator saves and restores any caller-saved registers that are live across a `call` instruction. It also saves and restores any used callee-saved registers in the function prologue/epilogue.
-   **Entry Point**: The final executable's entry point is `_start`. This label marks the beginning of the top-level code. If a `main` function is defined, `_start` will execute all top-level statements and then `call main`. The return value of `main` is used as the program's exit code. If no `main` exists, the program exits after the top-level code.

## Runtime ASM Library
-   **Output to file-descriptor**: `print_string`, `print_char`, `print_int`, `print_uint`, `print_newline`.
-   **Input from stdin**: `read_char`, `read_word`, `parse_uint`, `parse_int`.
-   **Memory Management**: `malloc`, `free`, `memcpy`, `memset`
    - Note that memory allocated with `malloc` is signed so that during `free` we know that we are deallocated our own memory.
-   **String Utilities**: `string_length`, `string_equals`, `string_copy`, `string_concat`.
-   **Other**: `exit`, `clrscr`.
