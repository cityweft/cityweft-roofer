# Cityweft fork of roofer

This is `cityweft/cityweft-roofer`: upstream [3DBAG/roofer](https://github.com/3DBAG/roofer) `main`
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
`https://github.com/cityweft/cityweft-roofer/releases/download/<tag>/roofer-linux-x86_64-<tag>.tar.gz`.

## Local build

Upstream's recipe (Conan 2 + CMake, see `README.md` and `docker/Dockerfile`). The Docker build is
the quickest path on a Mac:

```bash
docker build --platform linux/arm64 -t roofer-cw:dev -f docker/Dockerfile .
docker run --rm -v /path/to/data:/data roofer-cw:dev --lod22 /data/tiles /data/footprints.gpkg /data/out
```

## Measured (2026-10-26, arm64 Docker builds of this tree)

Raw roofer output, before any of our repairs (`3d-data-processing/tools/roofer_bench.py`), on IGN
LiDAR HD + BD TOPO: Lyon (2 tiles, 2,845 footprints, ~40 pts/m² in footprints) and Marseille
(2 tiles, 10,220 footprints). "Ramp" = LoD 2.2 top more than 2 m above roofer's own ceiling
(max of `h_roof_max`, `h_pc_98p`); "top MAE" = LoD 2.2 top against the point cloud's 98th
percentile. The control is this build with `--nodata-ceiling-weight 0`.

| | Lyon control | Lyon fork | Marseille control | Marseille fork |
|---|---|---|---|---|
| raw ramps | 143 | 81 | 187 | 125 |
| top MAE (m) | 1.213 | 1.081 | 1.571 | 1.543 |
| top within 2 m | 86.6 % | 89.1 % | 81.0 % | 81.6 % |
| top > 3 m under cloud | 4.7 % | 4.7 % | 13.4 % | 13.5 % |
| bad fits (rmse > 3 m) | 405 | 405 | 1127 | 1127 |

The penalty touches ~1 % of the buildings and only lowers roofs; it never created a ramp.
A ceiling taken from the heightfield instead of the points removed more ramps (66 / 82) but
flattened real roofs whose points had formed no plane (Marseille top MAE 1.571 → 1.607), hence
the point-based ceiling. Upstream's wippolder test set is unchanged by the penalty. PR #181 and
the other fixes do not change these numbers; they fix determinism, crashes and the LoD 1.x
sentinel floors.

Note on platforms: the released `v1.1.0-beta.1` binary (x86_64, GitHub Actions) and the same tag
built on arm64 in Docker give different raw results on Marseille (145 vs 188 ramps, 39 vs 14
glass-roof LoD 1.1 blocks, top MAE 1.60 vs 1.57 m) although each binary is deterministic run to
run (0 of 8,957 buildings differ between two runs). Floating-point differences between the
platforms feed the region growing; compare builds on one platform only. The production binary is
the x86_64 release build, like the beta the ingest image uses today.
