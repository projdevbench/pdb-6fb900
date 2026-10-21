#!/bin/bash
# Runs tab-separated "input<TAB>expected" cases through the interpreter, one
# REPL line each, and reports mismatches.
cd "$(dirname "$0")"
BIN=${BIN:-../build/code}
cases=${1:-readme_cases.txt}
cut -f1 "$cases" > /tmp/scm_in.txt
cut -f2 "$cases" > /tmp/scm_exp.txt
"$BIN" < /tmp/scm_in.txt | sed 's/scm> //g' > /tmp/scm_out.txt
paste -d'\t' /tmp/scm_in.txt /tmp/scm_exp.txt > /tmp/scm_pairs.txt
fail=0; n=0
while IFS= read -r got <&3 && IFS=$'\t' read -r input exp <&4; do
  n=$((n+1))
  if [ "$got" != "$exp" ]; then fail=$((fail+1)); echo "FAIL: $input | expected [$exp] got [$got]"; fi
done 3</tmp/scm_out.txt 4</tmp/scm_pairs.txt
echo "$((n-fail))/$n passed"
