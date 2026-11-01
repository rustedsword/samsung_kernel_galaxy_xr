#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/ubuntu-dev-install.sh [OPTION]... [HOST]

Install the kernel from OUT_DIR on the headset as XrBoot entry dev-RELEASE
and boot it once on the next reboot. HOST is the headset's ssh destination;
without it, install on this machine.

  --no-oneshot  don't boot it on the next reboot
  --reboot      reboot when done
  --remove      remove it instead

Environment: OUT_DIR, LLVM_BIN, as for scripts/build-galaxyxr.sh.
EOF
}

remove=0
oneshot=1
reboot=0
host=
for arg; do
    case $arg in
        --no-oneshot) oneshot=0 ;;
        --reboot) reboot=1 ;;
        --remove) remove=1 ;;
        -h | --help) usage; exit ;;
        -*) usage >&2; exit 1 ;;
        *) host=$arg ;;
    esac
done

source_dir=$(realpath "$(dirname "$0")/..")
output_dir=$(realpath "${OUT_DIR:-$source_dir/out/galaxyxr}")
[[ -z ${LLVM_BIN:-} ]] || PATH=$LLVM_BIN:$PATH
release=$(< "$output_dir/include/config/kernel.release")
if [[ ! $release =~ ^[0-9][A-Za-z0-9._+-]*$ ]]; then
    echo "bad kernel release: $release" >&2
    exit 1
fi

run() {
    if [[ -n $host ]]; then
        ssh "$host" sudo "$@"
    else
        sudo "$@"
    fi
}

copy() {
    if [[ -n $host ]]; then
        rsync -rl --checksum --delete --rsync-path='sudo rsync' "$1/" "$host:/$1/"
    else
        sudo rsync -rl --checksum --delete "$1/" "/$1/"
    fi
}

if (( ! remove )); then
    stage=$(mktemp -d)
    trap 'rm -rf "$stage"' EXIT
    cd "$stage"
    modules=usr/lib/modules/$release
    trees=usr/lib/linux-image-$release
    dts=$output_dir/arch/arm64/boot/dts
    make -s -C "$source_dir" O="$output_dir" ARCH=arm64 LLVM=1 LLVM_IAS=1 \
        CROSS_COMPILE=aarch64-linux-gnu- INSTALL_MOD_PATH="$stage/usr" \
        INSTALL_MOD_STRIP=1 DEPMOD=true modules_install
    rm "$modules/build" "$modules/source"
    image=Image
    if grep -qx 'CONFIG_EFI_ZBOOT=y' "$output_dir/.config"; then
        image=vmlinuz.efi
    fi
    install -m 644 "$output_dir/arch/arm64/boot/$image" "$modules/vmlinuz"
    cp "$source_dir"/Documentation/galaxyxr/samsung-build/modules.{list,vendor_blocklist}.anorak "$modules/"
    mkdir -p "$trees/vendor/qcom" "$trees/samsung/xrvst2"
    cp "$source_dir/debian/cmdline" "$trees/"
    cp "$dts"/vendor/qcom/*.dtb "$trees/vendor/qcom/"
    cp "$dts"/samsung/xrvst2/*.dtbo "$trees/samsung/xrvst2/"
    copy "$modules"
    copy "$trees"
fi

run kernel-install --entry-token=literal:dev remove "$release"
if (( remove )); then
    run rm -rf "/usr/lib/modules/$release" "/usr/lib/linux-image-$release"
    if [[ $(run cat /boot/xrboot/loader/oneshot 2> /dev/null) == "dev-$release" ]]; then
        run rm /boot/xrboot/loader/oneshot
    fi
else
    run kernel-install --entry-token=literal:dev add "$release" "/usr/lib/modules/$release/vmlinuz"
    if (( oneshot )); then
        echo "dev-$release" | run tee /boot/xrboot/loader/oneshot > /dev/null
        run sync -f /boot/xrboot/
    fi
fi
if (( reboot )); then
    run systemctl --no-block reboot
fi
