#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail

# Upstream llvmorg-23.1.4, pinned independently of the tag.
llvm_revision=7a941dfb2877a0b8cf30323fefa85ad019e0b0f5
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
tools_dir=${GALAXYXR_TOOLS_DIR:-"$source_dir/.tools"}
jobs=${JOBS:-8}

if [[ ${1:-} == --help ]]; then
    cat <<'EOF'
Usage: scripts/bootstrap-galaxyxr-llvm.sh

Download and build upstream LLVM 23.1.4 (Clang, LLD and LLVM tools).
Installs only under .tools/llvm-23.1.4; does not change system packages.
Requires git, CMake, Ninja, Python 3 and a working host C/C++ toolchain.
The kernel additionally needs the host tools checked by build-galaxyxr.sh.

Environment: GALAXYXR_TOOLS_DIR, JOBS, CC, CXX.
EOF
    exit 0
fi
[[ $# == 0 ]] || { printf 'Unexpected arguments. Use --help.\n' >&2; exit 1; }
for tool in git cmake ninja python3 "${CC:-cc}" "${CXX:-c++}"; do
    command -v "$tool" >/dev/null || {
        printf 'Missing bootstrap prerequisite: %s\n' "$tool" >&2
        exit 1
    }
done
mkdir -p -- "$tools_dir"
tools_dir=$(cd -- "$tools_dir" && pwd)
llvm_source="$tools_dir/src/llvm-23.1.4"
llvm_build="$tools_dir/build/llvm-23.1.4"
llvm_prefix="$tools_dir/llvm-23.1.4"
if [[ ! -e $llvm_source ]]; then
    git init -q "$llvm_source"
    git -C "$llvm_source" fetch --depth=1 \
        https://github.com/llvm/llvm-project.git "$llvm_revision"
    git -C "$llvm_source" checkout --detach FETCH_HEAD
fi
[[ $(git -C "$llvm_source" rev-parse HEAD) == "$llvm_revision" ]] || {
    printf 'Unexpected LLVM checkout revision: %s\n' "$llvm_source" >&2
    exit 1
}
[[ -z $(git -C "$llvm_source" status --porcelain) ]] || {
    printf 'LLVM checkout has local changes: %s\n' "$llvm_source" >&2
    exit 1
}
cmake -S "$llvm_source/llvm" -B "$llvm_build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$llvm_prefix" \
    -DLLVM_ENABLE_PROJECTS='clang;lld' \
    -DLLVM_TARGETS_TO_BUILD='AArch64;ARM;X86' \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_INCLUDE_BENCHMARKS=OFF \
    -DLLVM_ENABLE_ZLIB=OFF \
    -DLLVM_ENABLE_ZSTD=OFF \
    -DLLVM_ENABLE_LIBXML2=OFF \
    -DLLVM_PARALLEL_LINK_JOBS=2
cmake --build "$llvm_build" --parallel "$jobs"
cmake --install "$llvm_build" --strip
printf '\nBuild the kernel with:\nLLVM_BIN=%q scripts/build-galaxyxr.sh\n' \
    "$llvm_prefix/bin"
