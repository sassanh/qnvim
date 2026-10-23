---
name: qnvim-release
description: Eine qnvim-Version veröffentlichen oder ein installierbares Plugin-Archiv bauen – Version, CHANGELOG, Doku, CI-Builds für Linux/Windows/macOS, Archiv für „Plugin installieren…“. Verwenden, wenn ein Release, ein Paket oder ein Versions-Tag gewünscht ist.
---

# Release und Paketierung

## Vorbedingungen

- Alle Tests grün (Skill `qnvim-gui-testing`, Stufen 1–2; Stufe 3 manuell).
- Echter Build gegen das qtcreator_dev-Paket der Zielversion (Skill `qnvim-build`,
  Weg A). Der Compile-Check allein reicht **nicht** für ein Release.

## Schritte

1. `CHANGELOG.md`: Abschnitt „Unreleased“ in `[<Version>](…/tree/v<Version>) (<Datum>)`
   umbenennen, neuen leeren „Unreleased“-Abschnitt mit Compare-Link anlegen.
   Die Plugin-Version kommt aus `IDE_VERSION` des SDK (`QNVim.json.in`).
2. Doku prüfen: `README.md`, `docs/gui-mode.md`, `docs/limitations.md`,
   `docs/building.md` (Versionen, neue Einstellungen/Befehle).
3. Commit `docs: release v<Version>`; Tag `v<Version>` (nur auf Anweisung pushen).
4. CI: `.github/workflows/build_cmake.yml` baut beim Tag für alle Plattformen
   und hängt die Archive an (Details in `.github/workflows/README.md`;
   Paketierung in `tools/ci/Package.cmake`).

## Lokales Archiv (zum Testen bei Nutzern)

Genau wie die CI (`tools/ci/Package.cmake`): Qt Creators eigenes
`build_plugin.py` aus dem heruntergeladenen SDK baut und verpackt.

```sh
cmake -S . -B build            # lädt das SDK nach external/qtcreator/dist-<OS>-<Version>/
QTC=external/qtcreator/dist-$(uname -s)-<Version>
python3 "$QTC/share/qtcreator/scripts/build_plugin.py" \
    --name QNVim-<Version>-Linux-x64 --src . --build build-pkg \
    --qt-path /pfad/zu/Qt/6.x/gcc_64 --qtc-path "$QTC" --output-path .
```

(macOS: das Skript liegt unter `Qt Creator.app/Contents/Resources/scripts/`
bzw. `Qt Creator.sdk/share/qtcreator/scripts/`; `Package.cmake` sucht alle
Varianten.) Ergebnis ist ein `.7z`, das *Hilfe > Über Plugins > Plugin
installieren…* direkt annimmt.

## Nicht vergessen

- macOS: Retarget auf das gebündelte Qt (`tools/RetargetPluginQt.sh`), codesign.
- Neovim-Mindestversion in `README.md` prüfen (GUI-Modus: ≥ 0.10 getestet).
- Release-Notes auf Deutsch/Englisch nach Wunsch; Inhalt aus dem CHANGELOG.
