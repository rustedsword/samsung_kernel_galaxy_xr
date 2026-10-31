#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail

. /packaging/common.sh

for variable_name in TARGETS PPA_REMOTE GPG_KEY UNSIGNED NO_UPLOAD; do
    [ -n "${!variable_name+x}" ] || die "missing environment variable: $variable_name"
done
[ -f /work/source.tar ] || die "no /work/source.tar"
[ -d /out ] || die "no /out"

WORK=$(mktemp -d "/tmp/$SOURCE-publish.XXXXXX")
trap 'rm -rf -- "$WORK"' EXIT

verify_sha256_manifest() {
    local checksums
    checksums=$(awk '/^Checksums-Sha256:/ { inside = 1; next }
        inside && /^[^ ]/ { exit }
        inside && NF == 3 { print $1 "  " $3 }' "$1")
    [ -n "$checksums" ] || die "no SHA-256 checksums in $1"
    (cd "$(dirname -- "$1")" && printf '%s\n' "$checksums" | sha256sum --check --strict --quiet -)
}

build_source_package() {
    local series=$1 version=$2
    local dir=$WORK/$series
    local src=$dir/$SOURCE-$version
    install -d "$src"
    tar -C "$src" -xf /work/source.tar

    sed -i -E "1s/^($SOURCE) \([^)]*\) [^;]*;/\1 ($version) $series;/" "$src/debian/changelog"
    [ "$(dpkg-parsechangelog -l"$src/debian/changelog" -SVersion)" = "$version" ] ||
        die "cannot set the changelog version to $version"
    [ "$(dpkg-parsechangelog -l"$src/debian/changelog" -SDistribution)" = "$series" ] ||
        die "cannot set the changelog series to $series"

    local args=(-S -sa -d)
    if [ "$UNSIGNED" -eq 1 ]; then
        args+=(-us -uc)
    else
        args+=("-k$GPG_KEY")
    fi
    say "Creating the $series source package $version"
    (cd "$src" && dpkg-buildpackage "${args[@]}")

    local changes=$dir/${SOURCE}_${version}_source.changes
    local dsc=$dir/${SOURCE}_${version}.dsc
    [ -f "$changes" ] && [ -f "$dsc" ] || die "dpkg-buildpackage left no $changes or $dsc"
    say "Checking the checksums"
    verify_sha256_manifest "$dsc"
    verify_sha256_manifest "$changes"
    if [ "$UNSIGNED" -eq 0 ]; then
        say "Checking the signatures"
        gpg --batch --verify "$dsc"
        gpg --batch --verify "$changes"
    fi
    say "Checking that the source package extracts"
    dpkg-source --no-check -x "$dsc" "$dir/extracted" > /dev/null
    rm -rf -- "$dir/extracted" "$src"
    say "lintian"
    lintian --display-info "$changes"

    find "$dir" -maxdepth 1 -type f -exec cp -a -t /out {} +
    UPLOADS+=("$changes")
}

UPLOADS=()
for target in $TARGETS; do
    build_source_package "${target%%=*}" "${target#*=}"
done

if [ "$NO_UPLOAD" -eq 1 ]; then
    say "Upload skipped"
else
    for changes in "${UPLOADS[@]}"; do
        say "Uploading ${changes##*/} to $PPA_REMOTE"
        dput "$PPA_REMOTE" "$changes"
    done
fi
