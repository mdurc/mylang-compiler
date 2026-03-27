Statically-typed, compiled language.
This compiler is a complete rewrite of a previous attempt, designed from the ground up to prioritize a well-designed and easily extenisible architecture.

- [Syntax](docs/syntax.md)
    - [Grammar](docs/grammar.txt)
- [Compiler Architecture](docs/architecture.md)
- [Testing](docs/testing.md)
- [Todo](todo.txt)

### Motives
Purely for learning purposes and to build upon the experience that I've gotten since the last time that I tried to build a compiler. Hopefully this can eventually be designed well enough to self-host. There are still some unimplemented complex features, and has not been tested too extensibly in large programs, however it's still usable.
- Attempts to adopt C/Rust/Mojo-like semantics with ownership/borrowing semantics, however these are very loosely enforced due to complexity.
- Performs a recursive descent parse, following a CFG for the language. It would be an interesting experiment to write and implement an LL(1) or LR(k) grammar, though it is not suitable for this project.
- Targets x86-64 directly (System V ABI conventions)

**Primary focus in development:**
- Well-deisgned error handling systems within each module
    - With a corresponding logger system for LSP support
- Being able to print the state of the various modules within the compilation process (for testing and debugging).

### OS Support & Dependencies & Building
I am developing on an Apple M1 (14.8.3). As of now I haven't done extensive cross-compatibility with other machines, although this is a future task.

**Build Dependencies:**
- `g++` (built with C++20)
- `make`
- `nasm` (assembling the x86-64 output)
- `ld` (macOS linker)
- `cmake` (for snapshot-testing suite only)

Compile the project using the root Makefile:
```bash
make
```
This produces the `./compiler_build_files/mycompiler` binary which accepts arguments.

As described in my focuses in this project, I wanted to be able to inspect the output of any stage of the compiler. This is mostly for learning and debugging. It can be performing through the compiler's CLI:

```bash
./compiler_build_files/mycompiler [stage] <input.sn> [output_file]

--tokens: Output lexer tokens.
--ast: Output the Abstract Syntax Tree.
--symtab: Output the Symbol Table.
--ir: Output the 3-Address Code Intermediate Representation.
--asm: Output the generated NASM assembly.
--exe: Compile, assemble, and link into an executable.
--json: Export compiler data for the LSP.
--repl: Start an interactive REPL.
```
