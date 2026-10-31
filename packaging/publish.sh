#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail

. "$(dirname -- "${BASH_SOURCE[0]}")/common.sh"

PPA_REMOTE=${PPA_REMOTE:-ppa:lightofmysoul/gxr}
PPA_PACKAGES_URL=${PPA_PACKAGES_URL:-https://launchpad.net/~lightofmysoul/+archive/ubuntu/gxr/+packages}
GPG_KEY=${GPG_KEY_FINGERPRINT:-C21DA3A97F220CA130545721B179022596F6BFCD}
declare -A UBUNTU_VERSIONS=([resolute]=26.04 [stonking]=26.10)

NO_UPLOAD=0
UNSIGNED=0
ALLOW_DIRTY=0
SELECTED_SERIES=()

usage() {
    cat <<EOF
Usage: packaging/publish.sh [OPTIONS]

Builds a source package of $SOURCE (3.0 native, the tree at HEAD) per
Ubuntu series in the Ubuntu image (podman), checks it and uploads it,
signed, to $PPA_REMOTE; Launchpad builds the binaries.

The newest debian/changelog entry gives the version, KERNEL.ABI~YY.MM.N
for its series; every series gets KERNEL.ABI with its own ~YY.MM.N. ABI
names the release, KERNEL-ABI-lightofmysoul-galaxyxr: raise it with every
upload that changes the kernel.

  --series CODENAME  Publish to this series (default: stonking); repeatable.
  --no-upload        Build, sign and check, but don't upload.
  --unsigned         Build and check without the key; implies --no-upload.
  --allow-dirty      Use the working tree; only with --no-upload.
  --key FINGERPRINT  Signing key (default: $GPG_KEY).
  --ppa TARGET       dput target (default: $PPA_REMOTE).
  -h, --help         Show this help.

The artifacts stay in out/publish/. The host needs podman, git and gpg.
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --series)
            [ "$#" -ge 2 ] || die "--series needs an Ubuntu codename"
            SELECTED_SERIES+=("$2")
            shift 2
            ;;
        --no-upload)
            NO_UPLOAD=1
            shift
            ;;
        --unsigned)
            UNSIGNED=1
            NO_UPLOAD=1
            shift
            ;;
        --allow-dirty)
            ALLOW_DIRTY=1
            shift
            ;;
        --key)
            [ "$#" -ge 2 ] || die "--key needs a fingerprint"
            GPG_KEY="$2"
            shift 2
            ;;
        --ppa)
            [ "$#" -ge 2 ] || die "--ppa needs a dput target"
            PPA_REMOTE="$2"
            shift 2
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            die "unknown option: $1 (try --help)"
            ;;
    esac
done

[ "$ALLOW_DIRTY" -eq 0 ] || [ "$NO_UPLOAD" -eq 1 ] || die "--allow-dirty is only for --no-upload or --unsigned"
[ "${#SELECTED_SERIES[@]}" -gt 0 ] || SELECTED_SERIES=(stonking)
for series in "${SELECTED_SERIES[@]}"; do
    [ -n "${UBUNTU_VERSIONS[$series]+x}" ] || die "unknown Ubuntu series: $series"
done
for command_name in git podman sha256sum; do
    command -v "$command_name" > /dev/null || die "$command_name not found"
done
[ "$UNSIGNED" -eq 1 ] || command -v gpg > /dev/null || die "gpg not found"

status=$(git -C "$TREE" status --porcelain --untracked-files=no
    git -C "$TREE" status --porcelain --untracked-files=all -- debian packaging)
if [ -n "$status" ]; then
    if [ "$ALLOW_DIRTY" -eq 0 ]; then
        printf '%s\n' "$status" | sort -u >&2
        die "the tree has uncommitted changes: commit them, or check with --no-upload --allow-dirty"
    fi
    warn "building from the working tree"
fi

[ "$(< "$TREE/debian/source/format")" = "3.0 (native)" ] || die "debian/source/format must be 3.0 (native)"
header=$(sed -n 1p "$TREE/debian/changelog")
pattern='^([^[:space:]]+) \(([^)]*)\) ([^;[:space:]]+);'
[[ $header =~ $pattern ]] || die "cannot parse the first line of debian/changelog"
[ "${BASH_REMATCH[1]}" = "$SOURCE" ] || die "debian/changelog is not $SOURCE's"
version=${BASH_REMATCH[2]}
distribution=${BASH_REMATCH[3]}
[ -n "${UBUNTU_VERSIONS[$distribution]+x}" ] || die "debian/changelog targets an unknown series: $distribution"
kernel=$(sed -n 's/^\(VERSION\|PATCHLEVEL\|SUBLEVEL\) = //p' "$TREE/Makefile" | paste -s -d .)
pattern="^(${kernel//./\\.}\\.[0-9]+)~([0-9]+\\.[0-9]+)\\.([0-9]+)\$"
[[ $version =~ $pattern ]] && [ "${BASH_REMATCH[2]}" = "${UBUNTU_VERSIONS[$distribution]}" ] ||
    die "the version $version must be $kernel.ABI~${UBUNTU_VERSIONS[$distribution]}.N for $distribution"
base=${BASH_REMATCH[1]}
revision=${BASH_REMATCH[3]}

targets=()
for series in "${SELECTED_SERIES[@]}"; do
    targets+=("$series=$base~${UBUNTU_VERSIONS[$series]}.$revision")
done

gnupg=
cleanup() {
    [ -z "$gnupg" ] || rm -rf -- "$gnupg"
    [ -z "${run_dir:-}" ] || rm -rf -- "$run_dir/work"
}
trap cleanup EXIT

container_args=(run --rm)
if [ "$UNSIGNED" -eq 0 ]; then
    gnupg_home=${GNUPGHOME:-$HOME/.gnupg}
    gpg --homedir "$gnupg_home" --batch --list-secret-keys "$GPG_KEY" > /dev/null 2>&1 ||
        die "the secret key $GPG_KEY is not available"
    gnupg=$(mktemp -d "${TMPDIR:-/tmp}/$SOURCE-gnupg.XXXXXX")
    cp -a "$gnupg_home/." "$gnupg/"
    find "$gnupg" \( -name 'S.*' -o -name '*.lock' -o -name '.#lk*' \) -delete
    chmod 700 "$gnupg"
    container_args+=(-v "$gnupg:/root/.gnupg")
else
    warn "building unsigned source packages; nothing is uploaded"
fi

builder_image

run_dir=$TREE/out/publish/$SOURCE-$base-$(date -u +%Y%m%dT%H%M%SZ)
mkdir -p -- "$run_dir/work"
if [ -n "$status" ]; then
    tree_files | tar -C "$TREE" --null -T - -cf "$run_dir/work/source.tar"
else
    git -C "$TREE" archive --format=tar HEAD -- . ':(exclude)packaging' > "$run_dir/work/source.tar"
fi

say "Building $SOURCE $base for ${SELECTED_SERIES[*]}"
podman "${container_args[@]}" \
    -v "$run_dir/work:/work:ro" -v "$run_dir:/out" -v "$PACKAGING_DIR:/packaging:ro" \
    -e "TARGETS=${targets[*]}" -e "PPA_REMOTE=$PPA_REMOTE" -e "GPG_KEY=$GPG_KEY" \
    -e "UNSIGNED=$UNSIGNED" -e "NO_UPLOAD=$NO_UPLOAD" \
    "$IMAGE" bash /packaging/inside-container.sh

rm -rf -- "$run_dir/work"
changes=$(find "$run_dir" -maxdepth 1 -name "${SOURCE}_*_source.changes" | wc -l)
[ "$changes" -eq "${#targets[@]}" ] || die "expected ${#targets[@]} source .changes files, found $changes"
say "Artifacts: $run_dir"
(cd "$run_dir" && sha256sum -- *)
[ "$NO_UPLOAD" -eq 1 ] || say "Uploaded; watch $PPA_PACKAGES_URL"
