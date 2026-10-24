---
name: qnvim-build
description: Build qnvim, compile it against a Qt Creator version and install it — with the real qtcreator_dev package or offline with the compile check against the Qt Creator sources. Use when the plugin needs to be built, a build error is investigated, the target version is checked, or the plugin is installed into Qt Creator.
---

# Building qnvim

Two paths, depending on what is available. Always work from the repository root.

## A) Real build (release, loadable in Qt Creator)

Requires Qt 6.8+ (Qt Creator 20.0.x is built against Qt 6.11 — use a matching Qt
version), CMake ≥ 3.22, a C++20 compiler, and network access to download.qt.io.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target QNVim
```

- The target version is in `external/qtcreator/version.cmake`; on the first
  configure, `tools/DownloadQtCreator.cmake` downloads `qtcreator.7z` and
  `qtcreator_dev.7z` into `external/qtcreator/dist-<OS>-<version>/`.
- Existing Qt Creator SDK: `-DFETCH_QTC=OFF -DCMAKE_PREFIX_PATH=/path/to/qtcreator`.
- Result: Linux/Windows `build/lib/qtcreator/plugins/libQNVim.so|QNVim.dll`,
  macOS `build/Qt Creator.app/Contents/PlugIns/qtcreator/libQNVim.dylib`
  (then run `tools/RetargetPluginQt.sh`, see `docs/building.md`).
- Install: *Help > About Plugins > Install Plugin…* or copy the file into
  `~/.local/share/data/QtProject/qtcreator/plugins/<version>/` (Linux).
- macOS development loop: `./quick-run.sh`.

## B) Compile check (without qtcreator_dev, e.g. offline/CI/blocked downloads)

Compiles against the Qt Creator version's **source headers** and links the `.so`
with unresolved Qt Creator symbols. Proves API compatibility but does **not**
produce a loadable release binary.

```sh
QT_PREFIX=/path/to/Qt/6.11/gcc_64 tools/compilecheck/run.sh            # build only
QT_PREFIX=... tools/compilecheck/run.sh --tests --nvim "$(command -v nvim)"  # + tests
```

The script shallow-clones Qt Creator at tag `v<version>`, neovim-qt at the
pinned commit, and msgpack-c, and builds into `build-check/`. Only git is
needed — FetchContent archives (github.com/.../archive/...) may be blocked.

The harness lives in `tools/compilecheck/QtCreator/QtCreatorConfig.cmake`. If a
header is missing (e.g. new public include directories of a Qt Creator library),
add it to `_qtc_includes` there — use `PUBLIC_INCLUDES` in the `CMakeLists.txt`
of the respective library in the Qt Creator sources as a model.

No Qt available? conda-forge works without download.qt.io:
`micromamba create -p ~/qtenv -c conda-forge qt6-main=6.11 cmake ninja gxx_linux-64 nvim`
and then set `CC`/`CXX` to the conda compilers.

## Classifying build errors

| Symptom | Cause / solution |
|---|---|
| `KSyntaxHighlighting/State: No such file` | compile check only: forwarding headers are generated in the harness — check the list |
| `mimemagicrule_p.h` missing | `src/libs/utils/mimetypes2` missing from the includes |
| `X is not a member of Utils::Constants` | API changed → skill `qtc-api-port` |
| FetchContent `msgpackc` download fails | `-DFETCHCONTENT_SOURCE_DIR_MSGPACKC=<git-clone>` (`run.sh` does this automatically) |
| Plugin does not load, "version mismatch" | `CompatVersion` in `QNVim.json` ≠ Qt Creator version; build against the matching SDK |
| macOS: plugin does not load despite matching version | Retarget the Qt frameworks (`tools/RetargetPluginQt.sh`) |

## After the build

- Fix warnings in our own code (`src/`); `-Wall -Wextra` should be clean.
- For GUI changes, also run the `qnvim-gui-testing` skill.
