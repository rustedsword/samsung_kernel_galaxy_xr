# SPDX-License-Identifier: GPL-2.0-only

say() {
    printf '\033[1;34m==>\033[0m %s\n' "$*"
}

warn() {
    printf '\033[1;33mwarning:\033[0m %s\n' "$*" >&2
}

die() {
    printf '\033[1;31merror:\033[0m %s\n' "$*" >&2
    exit 1
}

PACKAGING_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
TREE=$(cd -- "$PACKAGING_DIR/.." && pwd -P)
SOURCE=linux-galaxyxr
IMAGE=linux-galaxyxr-builder

builder_image() {
    local inputs
    inputs=$(cat "$PACKAGING_DIR/Containerfile" "$TREE/debian/control" | sha256sum)
    inputs=${inputs%% *}
    if [ "$(podman image inspect --format '{{ index .Labels "org.linux-galaxyxr.inputs" }}' "$IMAGE" 2> /dev/null)" != "$inputs" ]; then
        say "Building the Ubuntu image $IMAGE"
        podman build --label "org.linux-galaxyxr.inputs=$inputs" --tag "$IMAGE" \
            --file "$PACKAGING_DIR/Containerfile" "$TREE/debian"
        podman image prune -f --filter label=org.linux-galaxyxr.inputs > /dev/null
    fi
}

tree_files() {
    git -C "$TREE" ls-files -z --cached -- . ':(exclude)packaging'
    git -C "$TREE" ls-files -z --others --exclude-standard -- debian
}
