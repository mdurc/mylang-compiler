**Statically-typed, compiled language. Compiles to a standalone x86-64 Mach-O binary.**

This compiler is a complete rewrite of a previous attempt, designed from the ground up to prioritize a well-designed and easily extenisible architecture.

- [Syntax](docs/syntax.md)
    - [Grammar](docs/grammar.txt)
    - The best place to see how the language works is in [sample_code/](sample_code/). This is where most testing, debugging, and experimenting occurs.
- **Standard library** [stdlib/](stdlib/): provides a collection of data structures and utilities that demonstrate some capabilities of the language. Because the language compiles to freestanding x86-64 Mach-O binaries, the standard library minimizes reliance on standard C runtimes, favoring direct XNU kernel syscalls. See [stdlib/usage-stdlib.sn](stdlib/usage-stdlib.sn) that shows how each module can be used.
- [Compiler Architecture](docs/architecture.md)
- [Testing](docs/testing.md)
- [Todo](todo.txt)

### Motives
Purely written as an exercise and to learn from the experiences I've gained since I last attempted a compiler. Goal is to eventaully be designed well enough to self-host without too much difficulty.
- Performs a recursive descent parse, following a CFG for the language. It would be an interesting experiment to write and implement an LL(1) or LR(k) grammar, though it is not suitable for this project.

**Primary focus in development:**
- Well-deisgned error handling systems within each module
    - With a corresponding logger system for LSP support
- Being able to print the state of the various modules within the compilation process (for testing and debugging).

### OS Support & Dependencies & Building
I am developing on an Apple M1 (14.8.3). As of now I haven't done extensive cross-compatibility with other machines, although this is a future task.

Because the language compiles to x86-64 binaries, running the generated output on Apple Silicon requires specific translation tools.

1. **Xcode Command Line Tools:** c++20 compiler `clang++`, `make`, and the macOS linker `ld`.
   ```bash
   xcode-select --install
   ```
2. **Homebrew Packages:** `nasm` for assembling x86-64; if you plan to run the test suite, you will also need `cmake` and `googletest`.
   ```bash
   brew install nasm                # for compiler source
   brew install cmake googletest    # for test suite compilation
   ```
3. **Rosetta 2 (apple silicon users):** Because the compiler outputs x86_64 Mach-O binaries, running your compiled `.exe` files on an M-series Mac natively will result in a `Bad CPU type in executable` error. You must install Rosetta 2, a background translation process that allows Apple Silicon to run x86_64 software:
   ```bash
   softwareupdate --install-rosetta --agree-to-license
   ```

**Building the Compiler:**
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
