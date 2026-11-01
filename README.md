# Samsung Galaxy XR kernel

Standalone Linux 5.10.240 source tree for the Samsung SM-I610,
based on firmware **I610UES3AZJ2**.

Contains various fixes and backports,

The default config is suitable for desktop Linux distros.

Build using installed Clang/LLVM and standard Linux host tools:

```sh
scripts/build-galaxyxr.sh
```

To install and boot the built kernel on a headset running Ubuntu:

```sh
scripts/ubuntu-dev-install.sh --help
```

For a local LLVM toolchain, run `scripts/bootstrap-galaxyxr-llvm.sh`.
