# Packaging

Ubuntu packages of this kernel, built in an Ubuntu 26.10 image with
podman (`Containerfile`, rebuilt when it or `debian/control` changes).

- `build-debs.sh`: `linux-image-RELEASE` and `linux-image-galaxyxr`
  from the working tree into `out/deb/`, then lintian. `-nc` reuses the
  previous build. On an arm64 Ubuntu machine, `dpkg-buildpackage -b` in
  the tree does the same without podman.
- `publish.sh`: signed source packages (3.0 native, the tree at HEAD)
  for `ppa:lightofmysoul/gxr`, checked (checksums, signatures,
  extraction, lintian) and uploaded with dput; Launchpad builds them.
  `--no-upload`, `--unsigned` and `--allow-dirty` stop short of that.

## Versions

`debian/changelog`'s newest entry is `KERNEL.ABI~YY.MM.N` for its
series, `5.10.240.1~26.10.1` for stonking. Each series gets `KERNEL.ABI`
with its own `~YY.MM.N`. ABI names the release,
`KERNEL-ABI-lightofmysoul-galaxyxr`, and so the package
`linux-image-RELEASE`: raise it for every upload that changes the
kernel, so the new kernel installs next to the old ones; raise N to
rebuild the same kernel.
