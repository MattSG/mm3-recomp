#!/usr/bin/env bash
# Runs every tests/*_regression.py (and test_*.py) and prints a pass/fail summary: the POSIX twin of
# run_regressions.ps1. Usage: tools/run_regressions.sh [--ci] [--filter pattern] [--timeout seconds]
#   --ci  skips the tests that need generated game code (tests/needs_generated.txt)
# Needs a C/C++ compiler on PATH ($CC is honoured) and the Python packages in tests/README.md.
set -u
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ci=0; filter=""; timeout_s=300
while [ $# -gt 0 ]; do
    case "$1" in
        --ci) ci=1 ;;
        --filter) filter="$2"; shift ;;
        --timeout) timeout_s="$2"; shift ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
    shift
done
py="${PYTHON:-python3}"
[ -x "$root/.venv-tests/bin/python" ] && py="$root/.venv-tests/bin/python"
export PYTHONPATH="$root"
logs="$(mktemp -d "${TMPDIR:-/tmp}/mm3-reg.XXXXXX")"
skip=""
[ "$ci" = 1 ] && skip="$(grep -v '^#' "$root/tests/needs_generated.txt" | grep -v '^$')"
pass=0; fail=()
for f in "$root"/tests/*_regression.py "$root"/tests/test_*.py; do
    [ -f "$f" ] || continue
    b="$(basename "$f")"
    case "$b" in *"$filter"*) ;; *) continue ;; esac
    if [ -n "$skip" ] && printf '%s\n' "$skip" | grep -qx "$b"; then continue; fi
    log="$logs/$b.log"
    if [[ "$b" == test_* ]]; then
        (cd "$root/tests" && timeout "$timeout_s" "$py" -m unittest "${b%.py}" >"$log" 2>&1)
    else
        (cd "$root" && timeout "$timeout_s" "$py" "$f" >"$log" 2>&1)
    fi
    rc=$?
    if [ $rc -eq 0 ]; then pass=$((pass + 1)); else
        fail+=("$b")
        echo "FAIL $b (exit $rc)  -> $log"
        tail -n 12 "$log" | sed 's/^/    /'
    fi
done
echo "passed=$pass failed=${#fail[@]}"
exit "${#fail[@]}"
