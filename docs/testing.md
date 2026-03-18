## Testing

### Snapshot testing

I found that snapshot testing is great for this kind of project, where it is hard to make unit tests due to the often-changing details of larger programs (and refactoring). Though I'm sure they could be designed, I am satisfied with the larger scale testing functions.
- Using CMake, Catch2, and ApprovalTests

From within the `testing` directory, you can run the make file to build the compiler and testing suite via Cmake.
```bash
make test

# for quick run without rebuilding
make quick_test
```

### Single-file debugging

I also have a more _on-the-fly_ testing setup that doesn't require the amount of modifications that the snapshot testing suite does. This is much more usable and simpler for quick debugging and running tests on a specific file.

From the root directory, you can run following commands (note that the makefile expects the test file to be named `tfile.sn`):
```bash
# generate the file that we want to build and run
echo "print 123;" > tfile.sn

# build the compiler, then create all submodule outputs, then run the executable
## Testing

### Snapshot testing

I found that snapshot testing is great for this kind of project, where it is hard to make unit tests due to the often-changing details of larger programs (and refactoring). Though I'm sure they could be designed, I am satisfied with the larger scale testing functions.
- Using CMake, Catch2, and ApprovalTests

From within the `testing` directory, you can run the make file to build the compiler and testing suite via Cmake.
```bash
make test

# for quick run without rebuilding
make quick_test
```

### Single-file debugging

I also have a more _on-the-fly_ testing setup that doesn't require the amount of modifications that the snapshot testing suite does. This is much more usable and simpler for quick debugging and running tests on a specific file.

From the root directory, you can run following commands (note that the makefile expects the test file to be named `tfile.sn`):
```bash
# generate the file that we want to build and run
echo "print 123;" > tfile.sn

# build the compiler, then create all submodule outputs, then run the executable
make test

# if we only want to build and create all submodule outputs
make update_test
```

These make commands will run the following, which creates a file for every stage of the compiler from this test file:
```bash
./compiler_build_files/mycompiler \
	--tokens tfile.sn tfile.sn.tokens \
	--ast tfile.sn tfile.sn.ast \
	--symtab tfile.sn tfile.sn.symtab \
	--ir tfile.sn tfile.sn.ir \
	--asm tfile.sn tfile.sn.asm \
	--exe tfile.sn tfile.sn.exe
```

make test

# if we only want to build and create all submodule outputs
make update_test
```

These make commands will run the following, which creates a file for every stage of the compiler from this test file:
```bash
./compiler_build_files/mycompiler \
	--tokens tfile.sn tfile.sn.tokens \
	--ast tfile.sn tfile.sn.ast \
	--symtab tfile.sn tfile.sn.symtab \
	--ir tfile.sn tfile.sn.ir \
	--asm tfile.sn tfile.sn.asm \
	--exe tfile.sn tfile.sn.exe
```

