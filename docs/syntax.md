## Syntax
The formal grammar is defined in EBNF format in `info/grammar.txt`.

### Variable Declarations

Variables are immutable by default. The `mut` keyword makes a variable mutable. Type can be explicit or inferred.

```
x: i64 = 10;    // Immutable, explicit type, initialized to 10

mut x: i32;     // Mutable, uninitialized (defaults to 0)

mut y: i32 = 3; // Mutable, explicit type + initializer
z: i32 = 5;     // Immutable, explicit type + initializer

mut a := 10;    // Mutable, inferred type
b := "hello";   // Immutable, inferred type

/* Declaration vs Assignment */
mut x := 3;         // inferred declaration by using :=
mut y : i32 = 2;    // explicit declaration using : T =

x = 13; // assignment using = (we cannot use := because it will re-declare the variable)
```

### Data Types

- **Primitive Types**: `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f64`, `bool`, `string`, `u0` (void/null type).
- **Structs**: User-defined aggregate data types. Only data fields are allowed.
- **Pointers**: Raw pointers to data in memory.
- **Function Types**: Types representing callable functions, used for first-class functions.

### Control Flow

- **If-Else**: Standard conditional branching. `else if` is supported.
    ```
    if (x > 0) {
      print "positive";
    } else if (x < 0) {
      print "negative";
    } else {
      print "zero";
    }
    ```
- **While Loop**:
    ```
    while (condition) { /* ... */ }
    ```
-   **For Loop**: C-style for loop with optional initializer, condition, and iteration expressions.
    ```
    for (mut i := 0; i < 10; i = i + 1) { /* ... */ }
    ```
- **Switch Statement**: Dispatches on the value of an expression. Supports `case` and a single `default`. There is no fall-through; each case has its own block scope.
    ```
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
    ```
    func add(a: i64, b: i64) returns (sum: i64) {
      return a + b;
    }
    foo : func(i64, i64)->i64 = add; // first class function
    ```
- **Return Values**: Functions can return a single value. The return variable (`sum`) must be declared in the signature and is implicitly available within the function body. A `return` statement can be used for an early exit. If no `return` is used, the final value of the named return variable is returned. Functions without a `returns` clause implicitly return `u0`.

- **External Functions**:
    - Use the `extern` keyword to declare a function signature without a body. This allows the compiler to type-check calls to functions that will be provided by the linker (e.g., from the x86_64 runtime library).
    - See `sample_code/strings.sn` for an example

    ```mylang
    extern func print_char(c: u8);
    extern func string_length(str: string) returns (len: i64);
    extern func malloc(size: i64) returns (ptr: ptr<mut u0>);
    ```

- **Parameters and Ownership**:
    - `imm` (default): Immutable borrow. The argument is passed by reference, but cannot be modified.
    - `mut`: Mutable borrow. The argument is passed by reference and can be modified. The passed variable must be mutable.
    - `take`: The function takes ownership of the argument. This results in a copy/move. The `give` keyword on the call-site can be used to signify an explicit move.

### Structs

- Stored on the heap, and must be freed with the use of the `free` keyword, as of now without any garbage collection system in place.
- RHS Structs in assignments and initialization are copied with `memcpy` except for when the RHS is a struct literal, as it is a new allocation to memory and has no need to be re-copied.

- **Declaration**:
    ```
    struct Point {
      x: i64,
      y: i64
    }
    ```
- **Instantiation (Literals)**:
    ```
    mut p := Point(x = 10, y = 20);
    ```
- **Field Access**:
    ```
    p.x = 15;
    print p.y;
    ```

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
    - `free ptr`: Frees a single object.
    - `free[] ptr`: Frees an array.
    - Note that in the current implementation, in both cases of allocation we are allocating a block of memory, so both of these versions perform the same free, though can be helpful for specificity to the writer.
- **Pointer Arithmetic**: Pointers support addition and subtraction with integers. Array-style subscripting `ptr[i]` is syntactic sugar for `*(ptr + i)`.

### Strings

- Stored on the heap as null-terminated byte sequences, and must be freed with the use of the `free` keyword, as of now without any garbage collection system in place.
- RHS Strings in assignments and initialization are copied with `string_copy` except for the case described with an immutable string variable.

- **String Literals**: Stored in the data section. When immutable variables are initialized with a string literal, no string copy is done due to the inability to modify the string anyways. For mutable variables a copy is done on the string literal within the data segment.
    ```
    x : string = "hello";  // String literal, stored in .data section
    ```
- **String Concatenation**: Uses the `+` operator and returns a heap-allocated string.
    ```
    a : string = "hello";
    b : string = "world";
    c : string = a + b; // helloworld
    ```
- **String Subscripting**: Access individual characters using `string[index]`, returning `u8` values.
    ```
    // loading
    s : string = "hello";
    h : u8 = s[0];       // 'h' (104 in ASCII)
    e : u8 = s[1];       // 'e' (101 in ASCII)

    // storing
    s[0] = 'a'; // character literal
    s[1] = 97;  // ascii
    ```

### Special Statements

- `read <lvalue>;`: Reads from stdin into a mutable integer or string variable.
- `print <expr>, ...;`: Prints one or more expressions to stdout.
- `asm { ... };`: Inlines raw x86-64 assembly code.
- `error "message";`: Prints a string to stdout and exits with code 1.
- `exit <int_literal>;`: Exits the program with the given integer code.
- `#include "..."`: Preprocessor file import
- `#define <identifier> ...`: Preprocessor definition
