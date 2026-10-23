---
name: qnvim-build
description: qnvim bauen, gegen eine Qt-Creator-Version kompilieren und installieren – mit dem echten qtcreator_dev-Paket oder offline mit dem Compile-Check gegen die Qt-Creator-Quellen. Verwenden, wenn das Plugin gebaut, ein Build-Fehler untersucht, die Zielversion geprüft oder das Plugin in Qt Creator installiert werden soll.
---

# qnvim bauen

Zwei Wege, je nachdem was verfügbar ist. Immer im Repo-Wurzelverzeichnis arbeiten.

## A) Echter Build (Release, ladbar in Qt Creator)

Braucht Qt 6.8+ (Qt Creator 20.0.x ist gegen Qt 6.11 gebaut – passende Qt-Version
nehmen), CMake ≥ 3.22, C++20-Compiler, Netz zu download.qt.io.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target QNVim
```

- Die Zielversion steht in `external/qtcreator/version.cmake`; beim ersten
  Configure lädt `tools/DownloadQtCreator.cmake` `qtcreator.7z` und
  `qtcreator_dev.7z` nach `external/qtcreator/dist-<OS>-<Version>/`.
- Vorhandenes Qt-Creator-SDK: `-DFETCH_QTC=OFF -DCMAKE_PREFIX_PATH=/pfad/zum/qtcreator`.
- Ergebnis: Linux/Windows `build/lib/qtcreator/plugins/libQNVim.so|QNVim.dll`,
  macOS `build/Qt Creator.app/Contents/PlugIns/qtcreator/libQNVim.dylib`
  (danach `tools/RetargetPluginQt.sh`, siehe `docs/building.md`).
- Installieren: *Hilfe > Über Plugins > Plugin installieren…* oder Datei in
  `~/.local/share/data/QtProject/qtcreator/plugins/<Version>/` (Linux) kopieren.
- macOS-Entwicklungsschleife: `./quick-run.sh`.

## B) Compile-Check (ohne qtcreator_dev, z. B. offline/CI/gesperrte Downloads)

Kompiliert gegen die **Quell-Header** der Qt-Creator-Version und linkt die `.so`
mit offenen Qt-Creator-Symbolen. Beweist API-Kompatibilität, ergibt aber
**keine** ladbare Release-Binary.

```sh
QT_PREFIX=/pfad/zu/Qt/6.11/gcc_64 tools/compilecheck/run.sh            # nur bauen
QT_PREFIX=... tools/compilecheck/run.sh --tests --nvim "$(command -v nvim)"  # + Tests
```

Das Skript klont per git (shallow) Qt Creator am Tag `v<Version>`, neovim-qt am
gepinnten Commit und msgpack-c, und baut nach `build-check/`. Nur git wird
gebraucht – FetchContent-Archive (github.com/.../archive/...) können gesperrt sein.

Das Harness liegt in `tools/compilecheck/QtCreator/QtCreatorConfig.cmake`. Fehlt
ein Header (z. B. neue öffentliche Include-Verzeichnisse einer Qt-Creator-Lib),
dort in `_qtc_includes` ergänzen – Vorbild ist `PUBLIC_INCLUDES` im
`CMakeLists.txt` der jeweiligen Lib in den Qt-Creator-Quellen.

Kein Qt vorhanden? conda-forge funktioniert ohne download.qt.io:
`micromamba create -p ~/qtenv -c conda-forge qt6-main=6.11 cmake ninja gxx_linux-64 nvim`
und dann `CC/CXX` auf die conda-Compiler setzen.

## Build-Fehler einordnen

| Symptom | Ursache / Lösung |
|---|---|
| `KSyntaxHighlighting/State: No such file` | nur Compile-Check: Forwarding-Header werden im Harness generiert – Liste prüfen |
| `mimemagicrule_p.h` fehlt | `src/libs/utils/mimetypes2` fehlt in den Includes |
| `X is not a member of Utils::Constants` | API geändert → Skill `qtc-api-port` |
| FetchContent `msgpackc` Download schlägt fehl | `-DFETCHCONTENT_SOURCE_DIR_MSGPACKC=<git-clone>` (macht `run.sh` automatisch) |
| Plugin lädt nicht, „version mismatch“ | `CompatVersion` in `QNVim.json` ≠ Qt-Creator-Version; gegen das passende SDK bauen |
| macOS: Plugin lädt nicht trotz passender Version | Qt-Frameworks retargeten (`tools/RetargetPluginQt.sh`) |

## Nach dem Build

- Warnungen in eigenem Code (`src/`) beheben; `-Wall -Wextra` sollte sauber sein.
- Für GUI-Änderungen zusätzlich Skill `qnvim-gui-testing` ausführen.
