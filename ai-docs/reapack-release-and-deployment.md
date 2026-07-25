# ReaPack Release and Deployment Plan

## Decision summary

Publish one ReaPack **native extension** package containing both the Klinke
extension binary and the bundled PlugMaps. ReaPack installs native extension
packages below REAPER's `UserPlugins` directory, which matches the locations
used by the extension:

- extension binary: `UserPlugins/`
- bundled PlugMaps: `UserPlugins/MCU/PlugMaps/`

Do **not** publish the PlugMaps as a `.data` package. ReaPack installs data
packages below REAPER's `Data/` directory, while `PlugMapManager` searches the
`UserPlugins/MCU/PlugMaps/` location for installed maps. User-created maps are
kept separately and must never be part of an update:

- Windows: `Documents/Reaper/MCU/PlugMaps/`
- macOS/Linux: `~/.config/REAPER/MCU/PlugMaps/`

This preserves the distinction already made by the code: shipped maps can be
updated by ReaPack; user maps remain untouched.

## Repository layout

Keep the source repository and the ReaPack metadata together initially. Do not
commit compiled binaries to Git. Upload them as immutable assets on a GitHub
Release (or equivalent release host).

```text
reapack/
  Klinke-MCU.ext             # ReaPack package definition and metadata
  PlugMaps/
    <plugin-name>.xml
    ...
```

The `.ext` file must live in a subdirectory because `reapack-index` does not
index package files placed at repository root. Its directory also becomes the
visible ReaPack category.

Illustrative package definition (the final binary names and repository URL
must match the release assets exactly):

```text
@version 0.9.5.0
@description Mackie Control Protocol (Klinke)
@author Steffen Klinke
@link Source https://github.com/<owner>/csurf_klinke_mcu
@metapackage
@provides
  [win64] reaper_csurf_mcu_klinke_x64.dll https://github.com/<owner>/csurf_klinke_mcu/releases/download/v$version/reaper_csurf_mcu_klinke_x64.dll
  [darwin64] reaper_csurf_mcu_klinke_x64.dylib https://github.com/<owner>/csurf_klinke_mcu/releases/download/v$version/reaper_csurf_mcu_klinke_x64.dylib
  [darwin-arm64] reaper_csurf_mcu_klinke_arm64.dylib https://github.com/<owner>/csurf_klinke_mcu/releases/download/v$version/reaper_csurf_mcu_klinke_arm64.dylib
  [linux64] reaper_csurf_mcu_klinke.so https://github.com/<owner>/csurf_klinke_mcu/releases/download/v$version/reaper_csurf_mcu_klinke.so
  PlugMaps/*.xml > MCU/PlugMaps/
```

The unqualified PlugMap line is installed on every platform. Each binary line
is platform-qualified, so a user downloads only the matching binary. The
extension package type makes the target paths relative to `UserPlugins`; thus
the PlugMap target becomes `UserPlugins/MCU/PlugMaps/`.

The examples intentionally include separate Intel and Apple-silicon macOS
assets. Alternatively, publish one tested universal `.dylib` and use one
`[darwin]` line. Do not advertise an architecture until it has been built and
tested. The current project only promises x64 Windows and x64 portable Linux;
macOS needs an Intel and ARM build (or a universal build) for full coverage.

## Versioning: release version versus build identity

There must be exactly one public release version, for example `0.9.6.0`.
Use that exact value in all of these places:

- `VERSION.txt` in the release commit;
- `@version` in `reapack/Klinke-MCU.ext`;
- the immutable Git tag and GitHub Release name, `v0.9.6.0`;
- every platform asset attached to that release.

ReaPack compares the package version, not a compiler build counter. Therefore
all platform artifacts for a release must be built from the **same commit** and
must retain the same public version.

### Current build-counter behaviour

At present, CMake rewrites the tracked `VERSION.txt` and increments its build
counter during every configure. This is useful for local development but is a
poor release identifier: configuring three independent builds may otherwise
produce three different displayed build numbers.

Until the build system is changed, use this safe release procedure:

1. In the release commit, set `VERSION.txt` to `<release-version> 0` and commit
   it. For example: `0.9.6.0 0`.
2. Tag that commit as `v0.9.6.0` **before** any release build changes its
   working tree.
3. On each build host, create a fresh clone or a separate clean worktree at
   that tag. Configure and build there. Each isolated build changes only its
   local checkout from `0` to `1`, so every artifact displays
   `v0.9.6.0 build 1`.
4. Never commit or push those configure-time `VERSION.txt` edits. Delete the
   temporary worktrees after the assets have been verified.

This answers the concern directly: pull/check out the same tag on every OS,
not the moving branch tip. The public version does not change. The working
trees become dirty, but the produced binaries agree on `build 1`.

### Recommended follow-up improvement

Before the first public release, change CMake so that configuring a build does
not modify a tracked source file. Keep `VERSION.txt` as the immutable public
release version and generate the displayed build identity in the build
directory from an explicit cache variable, e.g. `MCU_BUILD_ID`. A release job
would pass `-DMCU_BUILD_ID=1` on every platform; local builds could default to
`dev` or a Git short SHA. This removes dirty release worktrees and makes
reproducible builds straightforward. It is a small, separate build-system
change and should be tested before adopting it.

## Recommended release sequence

1. Prepare a release branch or release commit. Update `VERSION.txt`, the
   ReaPack `@version`, `@changelog`, PlugMaps, user documentation, and release
   notes together. Do not modify a released `@version` later.
2. Commit, test, and tag the exact source commit as `vX.Y.Z.W`. The tag is the
   source-of-truth input for every platform build.
3. Build from clean checkouts of that tag:
   - Windows: native x64 MSVC release build;
   - Linux: `scripts/build-portable-linux.sh` for the portable x64 artifact;
   - macOS: x86_64 plus arm64 builds, or one verified universal build.
4. Verify every artifact before publication: filename, CPU architecture,
   dynamic dependencies, exported REAPER entry point, and a smoke test in the
   matching REAPER installation. On macOS also test the real Gatekeeper/code
   signing path selected for distribution.
5. Create the GitHub Release for the already-existing tag, upload the
   immutable assets, and record checksums in the release notes. Do not replace
   assets after ReaPack users can see the version.
6. Run `reapack-index --check`; then generate/update `index.xml` with
   `reapack-index`, review the diff, commit it, and push it. ReaPack users
   import the raw public URL to `index.xml` once and receive future updates via
   Synchronize Packages.
7. In a clean REAPER profile on each supported OS, import the repository and
   install the package. Confirm the binary lands in `UserPlugins/`, bundled
   PlugMaps land in `UserPlugins/MCU/PlugMaps/`, and the extension lists those
   maps. Finally restart REAPER and confirm the surface loads.

The index should be published only after all referenced release URLs return
the intended immutable assets. A staged/prerelease tag such as
`v0.9.6.0-rc1` is useful when cross-platform validation needs time.

## Future automation

The manual process is appropriate for the first release. Later, use a tagged
CI workflow: build the three OS artifacts from the tag, run checks, attach the
assets to a draft release, update the ReaPack index only after the draft is
approved, then publish. The release version remains a human decision; CI must
never invent or increment it independently per platform.

## Sources consulted

- [ReaPack packaging documentation](https://github.com/cfillion/reapack-index/wiki/Packaging-Documentation): package types, metadata, platform selectors,
  `@provides`, target renaming, and version semantics.
- [reapack-index documentation](https://github.com/cfillion/reapack-index/wiki): repository checks, index generation, updates, and `--amend` behaviour.
- Project implementation: `src/modes/plugin/PlugMapManager.cpp`, which defines
  the installed and user PlugMap locations; `CMakeLists.txt`, which currently
  increments `VERSION.txt` while configuring.
