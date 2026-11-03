# Branches, tests and releases

This repository has two branch roles, and no third one.

| Branch | Purpose | What a push runs |
|---|---|---|
| `dev` | Development and integration. | For changes matching its path filters, `tests.yml` builds the host player, checks the device input driver and packer scripts, and runs the curated active test suites. Matching pull requests run the same workflow. No firmware image is built. |
| `personal` | Firmware releases for the maintainer's V1 and anyone flashing the published images. | `build-tempotec-v1.yml` cross-compiles and packages the firmware, advances the version, and creates a GitHub release with `v1.upt` and `v1_md5.txt`. |

`main` is not used. Nothing is merged into it and no workflow builds from it.

## Writing a change

Work on `dev`. A push that changes paths watched by `tests.yml` is the fast
feedback loop: it builds the host player, syntax-checks the device input
driver, runs the curated active test suites, and checks the packer scripts.
Changes outside those path filters do not start that workflow. If a matching
run is red, the branch is not ready to merge.

```bash
git switch dev
# ... work ...
git push origin dev
```

## Checks before a release

The test workflow is triggered by matching pushes to `dev`, manual dispatches,
and pull requests whose changed paths match its filters. It does **not**
currently run on `personal`, and the firmware workflow does not wait for or
invoke it. Review the `dev` test results before merging to `personal`; the
firmware build validates and packages the image but is not gated by `tests.yml`.

To run them by hand on a Linux host, from the repository root:

```bash
make -C sonix-player/tests all
```

That is the curated active list, in the same order, as the workflow. Every
target can also be built and run on its own, which is what to do while working
on one thing.

[sonix-player/tests/README.md](../sonix-player/tests/README.md) says what each one pins.

## Personal firmware and release

Merge `dev` into `personal` after checking the relevant test results. A push
to `personal` does **not** automatically rerun or wait for `tests.yml`. Instead,
the firmware workflow:

1. builds the image from the pushed commit with the MIPS toolchain;
2. advances the version and stamps the image with this repository as its update source;
3. creates a release whose body lists commits since the previous version and whose assets are `v1.upt` and `v1_md5.txt`.

The version normally advances by one patch. A commit message can request a minor or major bump. The run's Actions artifact additionally carries `BUILD-REPORT.txt`, the stock and output validation reports, and the player binaries; the build report names the exact commit that was packaged.

Download `v1.upt` and `v1_md5.txt` **from the same release**, and check the report before installing.

### A manual run

**Build TempoTec V1 firmware** can be dispatched for a branch, tag or commit. A manual run asks for:

- the **stock firmware URL** — the official `v1.upt` the image is built on;
- the **community-build acknowledgement** — the run refuses to start without it;
- optionally **`build_ref`**, the ref to build, and **`publish_release_tag`**, an existing release to upload the built image to. The workflow verifies that the tag names the commit it built before replacing any asset.

A manual run without `publish_release_tag` produces an Actions artifact and
nothing else. It does not automatically run the test workflow; run the tests
separately before relying on the image.

## Before installing

These are unofficial community builds for the TempoTec Variations V1. The workflow checks the package and confirms the stock kernel is unchanged, but it does not install the image or test it on physical hardware. Keep the official V1 `v1.upt` and its matching checksum available for recovery. Install the firmware and checksum together, and do not use the V1 package on another model.
