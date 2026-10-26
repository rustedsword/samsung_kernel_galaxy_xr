#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail

source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
output_dir=${OUT_DIR:-"$source_dir/out/galaxyxr"}
if [[ -n ${LLVM_BIN:-} ]]; then
    export PATH="$LLVM_BIN:$PATH"
fi

if [[ ${1:-} == --help ]]; then
    cat <<'EOF'
Usage: scripts/build-galaxyxr.sh [--check | --configure | make targets...]

Default targets: Image modules dtbs. Existing configurations are preserved.
--configure resets the output configuration to galaxyxr_defconfig.
--check only checks installed build tools and host development headers.

Environment: OUT_DIR, LLVM_BIN (directory containing clang), JOBS.
No tools are downloaded or installed by this script.
EOF
    exit 0
fi

missing=()
for tool in make clang clang++ ld.lld llvm-ar llvm-nm llvm-objcopy \
    llvm-objdump llvm-readelf llvm-strip flex bison bc perl python3 \
    openssl pkg-config rsync tar xz gzip awk sed find; do
    command -v "$tool" >/dev/null || missing+=("$tool")
done
if (( ${#missing[@]} )); then
    printf 'Missing build tools: %s\n' "${missing[*]}" >&2
    exit 1
fi
if ! printf '#include <openssl/bio.h>\n' |
    clang -x c -fsyntax-only -; then
    printf 'OpenSSL development headers are required.\n' >&2
    exit 1
fi
clang --version | head -n 1
[[ ${1:-} == --check ]] && exit 0

mkdir -p -- "$output_dir"
output_dir=$(cd -- "$output_dir" && pwd)
kmake=(make -C "$source_dir" "O=$output_dir" ARCH=arm64
    LLVM=1 LLVM_IAS=1 CROSS_COMPILE=aarch64-linux-gnu-)
if [[ ${1:-} == --configure ]]; then
    "${kmake[@]}" galaxyxr_defconfig
    exit 0
fi
if [[ ! -f $output_dir/.config ]]; then
    "${kmake[@]}" galaxyxr_defconfig
else
    "${kmake[@]}" olddefconfig
fi
jobs=${JOBS:-$(getconf _NPROCESSORS_ONLN)}
if [[ -z ${JOBS:-} ]] && (( jobs > 12 )); then
    jobs=12
fi
if (( $# == 0 )); then
    set -- Image modules dtbs
fi
exec "${kmake[@]}" "-j$jobs" "$@"
