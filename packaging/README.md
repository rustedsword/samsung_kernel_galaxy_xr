# Packaging

`publish.sh` builds signed source packages (3.0 native, the tree at
HEAD) for `ppa:lightofmysoul/gxr` in an Ubuntu image of each series
with podman (`Containerfile`, rebuilt when it or `debian/control`
changes), checks them (checksums, signatures, extraction, lintian) and
uploads them with dput (`inside-container.sh`); Launchpad builds them.

- `--test-build` first builds `linux-image-RELEASE` and
  `linux-image-galaxyxr` from the source package, cross on a PC and
  natively on arm64; they stay in `out/publish/` for `dpkg -i`.
- `--no-upload`, `--unsigned` and `--allow-dirty` stop short of the
  upload; `--allow-dirty --test-build --no-upload` builds the working
  tree.
- On an arm64 Ubuntu machine, `dpkg-buildpackage -b` in the tree builds
  the packages without podman.

## Versions

`debian/changelog`'s newest entry is `KERNEL.ABI~YY.MM.N` for its
series, `5.10.240.1~26.10.1` for stonking. Each series gets `KERNEL.ABI`
with its own `~YY.MM.N`. ABI names the release,
`KERNEL-ABI-lightofmysoul-galaxyxr`, and so the package
`linux-image-RELEASE`: raise it for every upload that changes the
kernel, so the new kernel installs next to the old ones; raise N to
rebuild the same kernel.
