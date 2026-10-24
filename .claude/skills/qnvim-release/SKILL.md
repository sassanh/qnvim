---
name: qnvim-release
description: Publish a qnvim release or build an installable plugin archive — version, CHANGELOG, docs, CI builds for Linux/Windows/macOS, archive for "Install Plugin…". Use when a release, a package or a version tag is requested.
---

# Release and packaging

## Prerequisites

- All tests green (skill `qnvim-gui-testing`, stages 1–2; stage 3 manual).
- A real build against the target version's qtcreator_dev package (skill
  `qnvim-build`, path A). The compile check alone is **not** enough for a
  release.

## Steps

1. `CHANGELOG.md`: rename the "Unreleased" section to
   `[<version>](…/tree/v<version>) (<date>)`, create a new empty "Unreleased"
   section with a compare link. The plugin version comes from the SDK's
   `IDE_VERSION` (`QNVim.json.in`).
2. Check docs: `README.md`, `docs/gui-mode.md`, `docs/limitations.md`,
   `docs/building.md` (versions, new settings/commands).
3. Commit `docs: release v<version>`; tag `v<version>` (only push when
   instructed).
4. CI: `.github/workflows/build_cmake.yml` builds for all platforms on the tag
   and attaches the archives (details in `.github/workflows/README.md`;
   packaging in `tools/ci/Package.cmake`).

## Local archive (for testing with users)

Same as CI (`tools/ci/Package.cmake`): Qt Creator's own `build_plugin.py` from
the downloaded SDK builds and packages it.

```sh
cmake -S . -B build            # downloads the SDK into external/qtcreator/dist-<OS>-<version>/
QTC=external/qtcreator/dist-$(uname -s)-<version>
python3 "$QTC/share/qtcreator/scripts/build_plugin.py" \
    --name QNVim-<version>-Linux-x64 --src . --build build-pkg \
    --qt-path /path/to/Qt/6.x/gcc_64 --qtc-path "$QTC" --output-path .
```

(macOS: the script is under `Qt Creator.app/Contents/Resources/scripts/` or
`Qt Creator.sdk/share/qtcreator/scripts/`; `Package.cmake` searches all
variants.) The result is a `.7z` that *Help > About Plugins > Install Plugin…*
accepts directly.

## Don't forget

- macOS: retarget to the bundled Qt (`tools/RetargetPluginQt.sh`), codesign.
- Check the minimum Neovim version in `README.md` (GUI mode: tested with ≥ 0.10).
- Release notes in German/English as desired; content from the CHANGELOG.
