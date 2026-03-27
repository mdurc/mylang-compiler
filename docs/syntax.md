## Syntax
The formal grammar is defined in EBNF format in `info/grammar.txt`.

The best place to see how the language works is in `sample_code/`. This is where I do most of my testing, debugging, and experimenting.

### Entrypoint

- The compiler will recognize a `main` function as the entrypoint if it exists.
    - If it exists, all global statements/expressions will be evaluated before calling main, despite the ordering.
- If you do not include a `main` function, the program will evaluate line-by-line from the top.

- Command line arguments can be received through the `main` function:
```mylang
/* read more about strings below */
func main(argc: i64, argv: ptr<string>) returns (rc: u8) ...
func main(argc: i64, argv: ptr<imm u8>) returns (rc: u8) ...
```

### Variable Declarations

Variables are immutable by default. The `mut` keyword makes a variable mutable. Type can be explicit or inferred.

```mylang
x: i64 = 10;    // Immutable, explicit type, initialized to 10

mut x: i32;     // Mutable, uninitialized (defaults to 0)

mut y: i32 = 3; // Mutable, explicit type + initializer
z: i32 = 5;     // Immutable, explicit type + initializer

mut a := 10;    // Mutable, inferred type
b := "hello";   // Immutable, inferred type (string/ptr<imm u8>)

/* Declaration vs Assignment */
mut x := 3;         // inferred declaration by using :=
mut y : i32 = 2;    // explicit declaration using : T =

x = 13; // assignment using = (we cannot use := because it will re-declare the variable)
```

### Data Types

- **Primitive Types**: `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f64`, `bool`, `u0` (void/null type).
- **Structs**: User-defined aggregate data types. Only data fields are allowed.
- **Pointers**: Raw pointers to data in memory.
- **Function Types**: Types representing callable functions, used for first-class functions.

### Control Flow

- **If-Else**: Standard conditional branching. `else if` is supported.
    ```mylang
    if (x > 0) {
      print "positive";
    } else if (x < 0) {
      print "negative";
    } else {
      print "zero";
    }
    ```
- **While Loop**:
    ```mylang
    while (condition) { /* ... */ }
    ```
-   **For Loop**: C-style for loop with optional initializer, condition, and iteration expressions.
    ```mylang
    for (mut i := 0; i < 10; i = i + 1) { /* ... */ }
    ```
- **Switch Statement**: Dispatches on the value of an expression. Supports `case` and a single `default`. There is no fall-through; each case has its own block scope.
    ```mylang
    switch (val) {
      case 1: { print "one"; }
      case 2: { print "two"; }
      default: { print "other"; }
    }
    ```
- **Loop Control**: `break` and `continue` are supported within `for` and `while` loops.

### Functions & Externs

Functions are top-level declarations.

- **Standard Declaration**:
    ```mylang
    func add_implicit_return(a: i64, b: i64) returns (sum: i64) { sum = a + b; }
    func add_explicit_return(a: i64, b: i64) returns (sum: i64) { return a + b; }
    foo : func(i64, i64)->i64 = add_explicit_return; // first class function
    ```
- **Return Values**: Functions can return a single value. The return variable (`sum`) must be declared in the signature and is implicitly available within the function body. A `return` statement can be used for an early exit. If no `return` is used, the final value of the named return variable is returned. Functions without a `returns` clause implicitly return `u0`.

- **External Functions**:
    - Use the `extern` keyword to declare a function signature without a body. This allows the compiler to type-check calls to functions that will be provided by the linker (e.g., from the x86_64 runtime library).
    - See `sample_code/strings.sn` for an example

    ```mylang
    extern func print_char(fd: i64, c: u8);
    extern func string_length(str: string) returns (len: i64);
    extern func malloc(size: i64) returns (ptr: ptr<mut u0>);
    ```

- **Parameters and Ownership**:
    - `imm` (default): Immutable borrow. The argument is passed by reference, but cannot be modified.
    - `mut`: Mutable borrow. The argument is passed by reference and can be modified. The passed variable must be mutable.
    - `take`: The function takes ownership of the argument. This results in a copy/move. The `give` keyword on the call-site can be used to signify an explicit move.

### Structs

- **Old model**: structs were allocated on the heap, and treated as pointers, which made passing them to functions/variables very simple. Unfortunately this is not very efficient, and also requires the user to manually `free` each struct that is created. And even worse, if somebody doesn't read the documentation, it is not clear that any allocation has occured, and a memory leak will likely occur.
- **New model**: they have been refactored to act as **value-types**, with statically sized stack allocations.

- **Sample usage of structs**: see `sample_code/struct-unit-tests.sn` 

- **Initialization**:
    - Compiler computes the `sizeof<StructType>` in the typechecker. This involved integrating byte-alignment checks into the typesystem so that the stack is correctly word-aligned. I tried to mimic the padding system that C uses.
    - Memset is performed when a struct is declared or instantiated (e.g., `Outer{}`).
    - After the memset, field initializers that were loaded into stack memory are memcopied into the variable space.

- **Pass-by-value**:
    - This, along with return-by-value proved to be a more complex feature than I originally suspected.
    - When an aggregate type is passed to a function by value, the _caller_ will allocate a temporary copy of the struct on its own stack frame.
    - The stack address of this copy is passed as a pointer as an argument register. The callee will then map this incoming pointer to a local parameter variable, executing a `memcpy` to pull the data into its own stack-frame.
- **Return-by-value**:
    - The _caller_ will allocate stack space for the expected return value.
    - The _caller_ will pass a pointer to this stack address as the first argument (`rdi`), and all regular function arguments are shifted over by one register.
    - The _callee_ then retrieves the argument and stores it in a local stack variable and eventually executes a `memcpy` to write data into the caller-provided memory address. The address is then returned in `rax`.


### Pointers and Memory Management

Memory are managed manually on the heap using `new` and `free`.

- **Pointer Types**: `ptr<T>` or `ptr<mut T>`. The `mut` indicates whether the pointed-to data is mutable.
- **Address-Of Operator (`&`)**:
    - `&x`: Takes an immutable reference, resulting in a `ptr<imm T>`.
    - `&mut x`: Takes a mutable reference to a mutable variable, resulting in a `ptr<mut T>`.
- **Dereference Operator (`*`)**: Accesses the data a pointer points to.
- **Heap Allocation (`new`)**:
    - `new<T>(value)`: Allocates a single object of type `T` on the heap and initializes it.
    - `new<T>[size]`: Allocates an array of `T`s on the heap.
- **Deallocation (`free`)**:
    - `free ptr`
- **Pointer Arithmetic**: Pointers support addition and subtraction with integers. Array-style subscripting `ptr[i]` is syntactic sugar for `*(ptr + sizeof<T>)`.

### Strings

- **Old model**: Stored on the heap as null-terminated byte sequences, and must be freed with the use of the `free` keyword.
- **New model**: reduced to raw memory pointers to immutable bytes to avoid the complexity of hidden heap allocations from string copies and operations.

- **Sample usage of strings**: see `sample_code/strings.sn`  

- **Implementation details**
    - **String Literals**: a string literal (e.g., "hello"), the characters are embedded directly into the .data section of the final assembly binary.
    - **Null-termination**: strings are strictly null-terminated.
    - **Dynamic Strings**: if a user wants to construct a string dynamically (concatenation), they must explictly allocate a mutable byte buffer on the heap (`new <mut u8>[len+1]`) with a null terminator.

### Special Statements

- `read <lvalue>;`: Reads from stdin into a mutable integer or string variable.
- `print <expr>, ...;`: Prints one or more expressions to stdout.
- `asm { ... };`: Inlines raw x86-64 assembly code.
- `error "message";`: Prints a string to stdout and exits with code 1.
- `exit <int_literal>;`: Exits the program with the given integer code.
- `#include "..."`: Preprocessor file import
- `#define <identifier> ...`: Preprocessor definition
