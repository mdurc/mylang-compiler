#!/bin/zsh

# ./run.sh (test.s default)
# ./run.sh test_string.s

TEST_FILE="${1:-test.s}"
TEMP_FILE=".temp_build.s"

echo ".set TRACK_MEMORY, 1" > "$TEMP_FILE"
echo ".set TARGET_MACOS, 1" >> "$TEMP_FILE"
cat ../runtime.s "$TEST_FILE" >> "$TEMP_FILE"

as -arch arm64 "$TEMP_FILE" -o test.o
if [ $? -ne 0 ]; then
  echo "assembly failed"
  rm -f "$TEMP_FILE"
  exit 1
fi

ld test.o -e _start -lSystem -syslibroot $(xcrun --sdk macosx --show-sdk-path)
if [ $? -ne 0 ]; then
  echo "linking failed"
  rm -f "$TEMP_FILE" test.o
  exit 1
fi

rm -f "$TEMP_FILE" test.o

./a.out
