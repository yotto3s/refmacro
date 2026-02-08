#!/usr/bin/env bash
# Run clang-format on all project source files.
#   ./scripts/run-clang-format.sh          # fix in-place
#   ./scripts/run-clang-format.sh --check  # CI: dry-run, exit 1 on diff
#   ./scripts/run-clang-format.sh file.hpp # format specific files

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

MODE="fix"
FILES=()

for arg in "$@"; do
    case "$arg" in
        --check) MODE="check" ;;
        *) FILES+=("$arg") ;;
    esac
done

# If no explicit files, find all .hpp/.cpp in project directories
if [[ ${#FILES[@]} -eq 0 ]]; then
    mapfile -t FILES < <(
        find "$REPO_ROOT/include/refmacro" \
             "$REPO_ROOT/tests" \
             "$REPO_ROOT/examples" \
            -type f \( -name '*.hpp' -o -name '*.cpp' \) \
            2>/dev/null | sort
    )
fi

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "No source files found."
    exit 0
fi

if [[ "$MODE" == "check" ]]; then
    echo "Checking formatting (${#FILES[@]} files)..."
    clang-format --dry-run --Werror "${FILES[@]}"
    echo "All files formatted correctly."
else
    echo "Formatting ${#FILES[@]} files in-place..."
    clang-format -i "${FILES[@]}"
    echo "Done."
fi
