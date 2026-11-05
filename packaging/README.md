# Packaging

`publish.sh` builds signed source packages (3.0 native, the tree at
HEAD) for `ppa:lightofmysoul/gxr` in an Ubuntu image of each series
with podman (`Containerfile`, rebuilt when it or `debian/control`
changes), checks them (checksums, signatures, extraction, lintian) and
uploads them with dput (`inside-container.sh`); Launchpad builds them.
Temporary build files also live under `out/publish/`, so space for the
kernel build is needed on that filesystem rather than in container storage.

- `--test-build` first builds the packages from the source package,
  natively on arm64 or cross on a PC; they stay in `out/publish/` for
  `dpkg -i`. The cross build leaves out `linux-headers-RELEASE` and
  the metapackages that need it (`-Pcross`): its tools would be the
  PC's.
- `--no-upload`, `--unsigned` and `--allow-dirty` stop short of the
  upload; `--allow-dirty --test-build --no-upload` builds the working
  tree.
- On an arm64 Ubuntu machine, `dpkg-buildpackage -b` in the tree builds
  the packages without podman.

## Versions

`debian/changelog`'s newest entry is `KERNEL.ABI~YY.MM.N` for its
series, `5.10.240.1~26.10.1` for stonking. Each series gets `KERNEL.ABI`
with its own `~YY.MM.N`. ABI names the release,
`KERNEL-ABI-lightofmysoul-galaxyxr`, and so the packages
`linux-{image,modules,headers}-RELEASE`: raise it for every upload that
changes the kernel, so the new kernel installs next to the old ones;
raise N to rebuild the same kernel.

## Packages

Like Ubuntu's kernels:

- `linux-image-RELEASE`: the kernel, its command line and device trees.
- `linux-modules-RELEASE`: the modules, `System.map` and the config.
- `linux-headers-RELEASE`: what building modules needs
  (`/usr/lib/modules/RELEASE/build`); build them with `LLVM=1`, as DKMS
  does by itself for clang kernels.
- `linux-image-galaxyxr`, `linux-headers-galaxyxr` and `linux-galaxyxr`
  (both) follow the latest release.
