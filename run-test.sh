#!/bin/zsh

# ./run-test.sh [--review|-r] [--yes|-y] (run all tests)
# ./run-test.sh [--review|-r] [--yes|-y] sample_code/fizzbuzz.sn

REVIEW_MODE=0
AUTO_CONTINUE=0
TARGET_FILES=()

while [[ $# -gt 0 ]]; do
  case $1 in
    --review|-r)
      REVIEW_MODE=1
      shift
      ;;
    --yes|-y)
      AUTO_CONTINUE=1
      shift
      ;;
    *)
      TARGET_FILES+=("$1")
      shift
      ;;
  esac
done

DONT_TEST=(
  "sample_code/conways/*"
  "sample_code/includes/*"
  "sample_code/inclusion-diamond/*"
  "sample_code/mergesort-visual.sn"
  "sample_code/read-stdin-output.sn"
  "*-ERR.sn"
)
IGNORE_PATTERN="(${(j:|:)DONT_TEST})"

INCLUDE=(
  "stdlib/usage-stdlib.sn"
)

normalize_output() {
  local in_file=$1
  local out_file=$2
  sed -E \
    -e 's/Address: [0-9]+/Address: <PTR>/g' \
    -e 's/[xX]86_64/<ARCH>/g' \
    -e 's/[aA]arch64/<ARCH>/g' \
    "$in_file" > "$out_file"
}

if (( ${#TARGET_FILES[@]} > 0 )); then
  files=("${TARGET_FILES[@]}")
else
  files=(sample_code/**/*.sn(N) ${INCLUDE[@]})
fi

PASSED_COUNT=0
FAILED_COUNT=0

for file in $files; do
  [[ -f "$file" ]] || continue
  if [[ $file == ${~IGNORE_PATTERN} || ${file:t} == ${~IGNORE_PATTERN} ]]; then
    echo "==> Skipping: $file"
    continue
  fi

  echo "==> Testing: $file"
  cp "$file" tfile.sn

  if make test-x86 > /dev/null 2>&1; then
    ./tfile.sn.exe > x86_out.txt 2>&1
  else
    echo "==> [x86_64] Build Failed"
    ((FAILED_COUNT++))
    continue
  fi

  if make test-arm > /dev/null 2>&1; then
    ./tfile.sn.exe > arm_out.txt 2>&1
  else
    echo "==> [Aarch64] Build Failed"
    ((FAILED_COUNT++))
    continue
  fi

  normalize_output x86_out.txt x86_norm.txt
  normalize_output arm_out.txt arm_norm.txt

  mv x86_norm.txt x86_out.txt
  mv arm_norm.txt arm_out.txt

  REVIEW_MSG=""
  if cmp -s x86_out.txt arm_out.txt; then
    echo "==> MATCHING OUTPUT"
    REVIEW_MSG="+echohl MoreMsg | echo ' MATCHING OUTPUT ' | echohl None"
    ((PASSED_COUNT++))
  else
    echo "==> DIVERGING OUTPUT **FAIL**"
    REVIEW_MSG="+echohl ErrorMsg | echo ' DIVERGING OUTPUT **FAIL** ' | echohl None"
    ((FAILED_COUNT++))
  fi

  if [[ $REVIEW_MODE -eq 1 ]]; then
    nvim "$file" "+rightbelow vsp x86_out.txt" "+set nomodifiable" "+rightbelow sp arm_out.txt" "+set nomodifiable" "+wincmd h" "$REVIEW_MSG"
    if [[ $AUTO_CONTINUE -eq 0 ]]; then
      read -q "REPLY?Press [Y] to continue to next file, [N] to abort: "
      echo ""
      [[ $REPLY =~ ^[Yy]$ ]] || break
    fi
  fi
done

rm -f x86_out.txt arm_out.txt

echo ""
echo "$PASSED_COUNT/$((PASSED_COUNT + FAILED_COUNT)) Passed"
