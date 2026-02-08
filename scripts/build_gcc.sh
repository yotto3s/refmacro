#!/usr/bin/env bash
# build_gcc.sh — Clone and build GCC from source with C++26 reflection support
#
# Usage: build_gcc.sh [OPTIONS]
#
# Options:
#   --gcc-repo URL           Git repository URL (default: https://github.com/gcc-mirror/gcc.git)
#   --gcc-branch BRANCH      Branch to checkout (default: master)
#   --gcc-commit COMMIT      Pin to specific commit (triggers full clone)
#   --source-dir DIR         Where to clone GCC source (default: /opt/gcc-src)
#   --build-dir DIR          Where to build GCC (default: /opt/gcc-build)
#   --install-prefix DIR     Where to install GCC (default: /opt/gcc)
#   --jobs N                 Parallel build jobs (default: $(nproc))
#   --skip-clone             Reuse existing source directory
#   --skip-install           Build only, no make install
#   --languages LANGS        Comma-separated languages (default: c,c++)
#   --help                   Show this help message

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

# Defaults
GCC_REPO="https://github.com/gcc-mirror/gcc.git"
GCC_BRANCH="master"
GCC_COMMIT=""
SOURCE_DIR="/opt/gcc-src"
BUILD_DIR="/opt/gcc-build"
INSTALL_PREFIX="/opt/gcc"
JOBS="$(nproc)"
SKIP_CLONE=false
SKIP_INSTALL=false
LANGUAGES="c,c++"

info()  { echo -e "${CYAN}[INFO]${RESET} $*"; }
ok()    { echo -e "${GREEN}[OK]${RESET} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${RESET} $*"; }
error() { echo -e "${RED}[ERROR]${RESET} $*" >&2; }
die()   { error "$@"; exit 1; }

usage() {
    # Print the header comment block as usage
    sed -n '2,/^$/{ s/^# \?//; p }' "$0"
    exit 0
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --gcc-repo)        GCC_REPO="$2"; shift 2 ;;
            --gcc-branch)      GCC_BRANCH="$2"; shift 2 ;;
            --gcc-commit)      GCC_COMMIT="$2"; shift 2 ;;
            --source-dir)      SOURCE_DIR="$2"; shift 2 ;;
            --build-dir)       BUILD_DIR="$2"; shift 2 ;;
            --install-prefix)  INSTALL_PREFIX="$2"; shift 2 ;;
            --jobs)            JOBS="$2"; shift 2 ;;
            --skip-clone)      SKIP_CLONE=true; shift ;;
            --skip-install)    SKIP_INSTALL=true; shift ;;
            --languages)       LANGUAGES="$2"; shift 2 ;;
            --help|-h)         usage ;;
            *) die "Unknown option: $1" ;;
        esac
    done
}

clone_gcc() {
    if [[ "$SKIP_CLONE" == true ]]; then
        if [[ ! -d "$SOURCE_DIR" ]]; then
            die "Source directory $SOURCE_DIR does not exist (--skip-clone)"
        fi
        info "Reusing existing source at $SOURCE_DIR"
        return
    fi

    if [[ -d "$SOURCE_DIR" ]]; then
        warn "Removing existing source directory: $SOURCE_DIR"
        rm -rf "$SOURCE_DIR"
    fi

    if [[ -n "$GCC_COMMIT" ]]; then
        info "Cloning GCC (full history for commit checkout)..."
        git clone --branch "$GCC_BRANCH" "$GCC_REPO" "$SOURCE_DIR"
        info "Checking out commit $GCC_COMMIT"
        git -C "$SOURCE_DIR" checkout "$GCC_COMMIT"
    else
        info "Shallow-cloning GCC (branch: $GCC_BRANCH)..."
        git clone --depth=1 --branch "$GCC_BRANCH" "$GCC_REPO" "$SOURCE_DIR"
    fi

    ok "GCC source cloned to $SOURCE_DIR"
}

configure_gcc() {
    mkdir -p "$BUILD_DIR"

    info "Configuring GCC (languages: $LANGUAGES)..."
    cd "$BUILD_DIR"

    "$SOURCE_DIR/configure" \
        --prefix="$INSTALL_PREFIX" \
        --enable-languages="$LANGUAGES" \
        --disable-bootstrap \
        --disable-multilib \
        --disable-libsanitizer \
        --disable-libvtv \
        --disable-libgomp \
        --with-system-zlib

    ok "GCC configured"
}

build_gcc() {
    info "Building GCC with $JOBS parallel jobs..."
    make -C "$BUILD_DIR" -j"$JOBS"
    ok "GCC build complete"
}

install_gcc() {
    if [[ "$SKIP_INSTALL" == true ]]; then
        info "Skipping install (--skip-install)"
        return
    fi

    info "Installing GCC to $INSTALL_PREFIX..."
    make -C "$BUILD_DIR" install
    ok "GCC installed to $INSTALL_PREFIX"
}

verify_gcc() {
    local gxx

    if [[ "$SKIP_INSTALL" == true ]]; then
        gxx="$BUILD_DIR/gcc/xg++"
        local bflag="-B$BUILD_DIR/gcc"
    else
        gxx="$INSTALL_PREFIX/bin/g++"
        local bflag=""
    fi

    if [[ ! -x "$gxx" ]]; then
        die "Compiler not found at $gxx"
    fi

    info "Verifying compiler..."
    "$gxx" $bflag --version | head -1

    info "Testing -freflection support..."
    local test_file
    test_file=$(mktemp /tmp/gcc_reflection_test_XXXXXX.cpp)
    trap "rm -f '$test_file' '${test_file%.cpp}'" EXIT

    cat > "$test_file" <<'CPP'
static_assert(^^int == ^^int);
static_assert(^^int != ^^const int);
int main() {}
CPP

    local compile_output
    if compile_output=$("$gxx" $bflag -std=c++26 -freflection "$test_file" -o "${test_file%.cpp}" 2>&1); then
        ok "C++26 reflection support verified"
    else
        error "Compilation output:"
        echo "$compile_output"
        die "Compiler does not support -freflection"
    fi
}

main() {
    parse_args "$@"

    echo -e "${BOLD}GCC Build Script${RESET}"
    echo -e "  Repository:     $GCC_REPO"
    echo -e "  Branch:         $GCC_BRANCH"
    [[ -n "$GCC_COMMIT" ]] && echo -e "  Commit:         $GCC_COMMIT"
    echo -e "  Source:         $SOURCE_DIR"
    echo -e "  Build:          $BUILD_DIR"
    echo -e "  Install prefix: $INSTALL_PREFIX"
    echo -e "  Jobs:           $JOBS"
    echo -e "  Languages:      $LANGUAGES"
    echo ""

    clone_gcc
    configure_gcc
    build_gcc
    install_gcc
    verify_gcc

    echo ""
    ok "GCC build pipeline complete"
}

main "$@"
