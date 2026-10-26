# Samsung Galaxy XR kernel

Standalone Linux 5.10.240 source tree for the Samsung SM-I610,
based on firmware **I610UES3AZJ2**.

Build using installed Clang/LLVM and standard Linux host tools:

```sh
scripts/build-galaxyxr.sh --check
scripts/build-galaxyxr.sh
```

The equivalent normal kernel commands are:

```sh
make O=out/galaxyxr ARCH=arm64 LLVM=1 LLVM_IAS=1 \
    CROSS_COMPILE=aarch64-linux-gnu- galaxyxr_defconfig
make O=out/galaxyxr ARCH=arm64 LLVM=1 LLVM_IAS=1 \
    CROSS_COMPILE=aarch64-linux-gnu- -j12 Image modules dtbs
```

The default enables KGSL, Kiwi v2 Wi-Fi, display/panels, audio,
camera, video, EVA, MMRM and the original Anorak/Samsung platform
modules. UH/RKP/KDP, PAC, BTI, MTE, CFI, shadow call stack, stack canaries,
FORTIFY, hardened usercopy, allocator hardening, SELinux, KASLR and automatic
stack/memory initialization are disabled. UBSAN, KASAN and KFENCE are
also disabled. LTO is also disabled.

For a local LLVM toolchain, run `scripts/bootstrap-galaxyxr-llvm.sh`.
