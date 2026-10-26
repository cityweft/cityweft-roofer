# Cityweft fork of roofer

This is `cityweft/roofer`: upstream [3DBAG/roofer](https://github.com/3DBAG/roofer) `main`
plus the open determinism PR (#181) and our own fixes. Branch `cityweft` is what we build;
`main` tracks upstream unchanged so rebases stay trivial.

What differs from upstream is listed under `[1.1.0-cw.1]` in `CHANGELOG.md`. Every change is
meant to go upstream as a PR; keep commits small and self-contained for that reason.

## Syncing upstream

```bash
git fetch upstream
git checkout main && git merge --ff-only upstream/main
git checkout cityweft && git merge main      # or rebase when the fork history is still short
```

## Releasing a binary for the ingest image

The upstream GitHub Actions workflow (`build_install_conan.yml`) builds linux-x86_64, macOS-arm64
and windows-x86_64 release assets when a tag starting with `v` is pushed, and needs a matching
`## [<version>] - <date>` section in `CHANGELOG.md` (the version is the tag without the `v`).

```bash
git tag v1.1.0-cw.1 && git push origin cityweft --tags
```

The ingest image (`3d-data-processing/docker/userdata.Dockerfile`) downloads
`https://github.com/cityweft/roofer/releases/download/<tag>/roofer-linux-x86_64-<tag>.tar.gz`.

## Local build

Upstream's recipe (Conan 2 + CMake, see `README.md` and `docker/Dockerfile`). The Docker build is
the quickest path on a Mac:

```bash
docker build --platform linux/arm64 -t roofer-cw:dev -f docker/Dockerfile .
docker run --rm -v /path/to/data:/data roofer-cw:dev --lod22 /data/tiles /data/footprints.gpkg /data/out
```
