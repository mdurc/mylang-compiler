#!/bin/zsh

TEST_FILE="${1:-test.asm}"
TEMP_FILE=".temp_build.asm"

echo "%define TRACK_MEMORY 1" > "$TEMP_FILE"
echo "%define TARGET_MACOS 1" >> "$TEMP_FILE"

cat ../runtime.asm "$TEST_FILE" >> "$TEMP_FILE"

nasm -f macho64 "$TEMP_FILE" -o test.o
if [ $? -ne 0 ]; then
  echo "assembly failed"
  rm -f "$TEMP_FILE"
  exit 1
fi

ld test.o -macos_version_min 10.13 -e _start -lSystem -no_pie \
-syslibroot `xcrun --sdk macosx --show-sdk-path`
if [ $? -ne 0 ]; then
  echo "linking failed"
  rm -f "$TEMP_FILE" test.o
  exit 1
fi

rm -f "$TEMP_FILE" test.o

./a.out
