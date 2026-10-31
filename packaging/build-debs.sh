#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail

. "$(dirname -- "${BASH_SOURCE[0]}")/common.sh"

if [ "${1:-}" = --help ]; then
    cat <<'EOF'
Usage: packaging/build-debs.sh [dpkg-buildpackage options...]

Builds linux-image-RELEASE and linux-image-galaxyxr from the working tree
(its tracked files and new files in debian/) with dpkg-buildpackage in the
Ubuntu image (podman), arm64, into OUT_DIR (default out/deb), then runs
lintian on them. -nc keeps the previous build.

Environment: OUT_DIR, JOBS.
EOF
    exit 0
fi

command -v podman > /dev/null || die "podman not found"
output_dir=${OUT_DIR:-"$TREE/out/deb"}
builder_image

mkdir -p -- "$output_dir/src"
tree_files | rsync -a --from0 --files-from=- "$TREE/" "$output_dir/src/"
rm -f -- "$output_dir/src/debian/debhelper-build-stamp"

jobs=${JOBS:-$(getconf _NPROCESSORS_ONLN)}
if [ -z "${JOBS:-}" ] && [ "$jobs" -gt 12 ]; then
    jobs=12
fi
podman run --rm -v "$output_dir:/build" -w /build/src "$IMAGE" \
    dpkg-buildpackage -a arm64 -b --no-sign "-j$jobs" "$@"

version=$(sed -n '1s/^[^ ]* (\([^)]*\)).*/\1/p' "$TREE/debian/changelog")
changes=${SOURCE}_${version#*:}_arm64.changes
say "lintian $changes"
podman run --rm -v "$output_dir:/build:ro" -w /build "$IMAGE" lintian --fail-on none "$changes"
