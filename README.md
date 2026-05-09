**Statically-typed, compiled language targeting x86-64 and AArch64. Compiles to a standalone Mach-O or ELF binary.**

This compiler is a complete rewrite of a previous attempt, designed from the ground up to prioritize a well-designed and easily extensible architecture.

- [Syntax](docs/syntax.md)
    - [Grammar](docs/grammar.txt)
    - The best place to see how the language works is in [sample_code/](sample_code/). This is where most testing, debugging, and experimenting occurs.
- **Standard library** [stdlib/](stdlib/): provides a collection of data structures and utilities that demonstrate some capabilities of the language. The language compiles to freestanding x86-64 and AArch64 binaries, favoring direct XNU/Linux kernel syscalls. See [stdlib/usage-stdlib.sn](stdlib/usage-stdlib.sn) that shows how each module can be used.
- [Compiler Architecture](docs/architecture.md)
- [Testing](docs/testing.md)

### Motives
Purely written as an exercise and to learn from the experiences I've gained since I last attempted a compiler. Goal is to eventually be designed well enough to self-host without too much difficulty.

**Primary focus in development:**
- Well-designed error handling systems within each module and a corresponding logger system to integrate LSP support.
- Cleanly supporting multiple target ISAs natively (x86_64 and AArch64).
- Being able to print the state of the various modules within the compilation process (for testing and debugging).
- Minimize the amount of dependencies used.

### OS Support & Dependencies & Building
I am developing on an Apple M1 (14.8.3). The compiler has been tested to work natively on **macOS (Darwin)** and **Linux (x86_64 & AArch64)**.


**MacOS (Apple Silicon) Toolchain**
1. Requires `clang++` (C++20), `make`, `nasm`, native macOS `ld`
   ```bash
   xcode-select --install
   brew install nasm

   # only required for test suite compilation
   brew install cmake googletest
   ```
2. *Rosetta 2* (only required if targeting x86_64)
    - Running compiled x86_64 Mach-O binaries on an M-series Mac natively will result in a `Bad CPU type in executable` error. You must install Rosetta 2, a background translation process, to test x86_64 output:
   ```bash
   softwareupdate --install-rosetta --agree-to-license
   ```

**Linux Toolchain**
1. `clang++`, `make`, `nasm`, `GNU ld`
    ```bash
    sudo apt update
    sudo apt install -y build-essential clang nasm binutils make

    sudo pacman -Syu
    sudo pacman -S base-devel clang nasm binutils
    ```

**Building the Compiler:**
Compile the project using the root Makefile:
```bash
make
```
This produces the `./compiler_build_files/mycompiler` binary which accepts arguments.

As described in my focuses in this project, I wanted to be able to inspect the output of any stage of the compiler. This is mostly for learning and debugging. It can be performing through the compiler's CLI:

```bash
./compiler_build_files/mycompiler [target] [stage] <input.sn> [output_file]

--track-memory  : Output a heap summary to stderr upon exit
* Because this compiler uses a custom, embedded runtime library instead of linking against libc, it includes a built-in memory tracker.

--target=macos  : Force Mach-O/Darwin ABI (default for macOS)
--target=linux  : Force ELF/SYSV ABI (default for Linux)
--target=aarch64  : Target ARM64 architecture (default for Apple Silicon)
--target=x86_64   : Target x86-64 architecture

--tokens: Output lexer tokens.
--ast: Output the Abstract Syntax Tree.
--symtab: Output the Symbol Table.
--ir: Output the 3-Address Code Intermediate Representation.
--asm: Output the generated NASM assembly.
--exe: Compile, assemble, and link into an executable.
       * a.out: Mach-O 64-bit executable x86_64
       * a.out: ELF 64-bit LSB executable, x86-64, version 1 (SYSV), statically linked not, stripped
--json: Export compiler data for the LSP.
--repl: Start an interactive REPL. (MacOS only)
```
