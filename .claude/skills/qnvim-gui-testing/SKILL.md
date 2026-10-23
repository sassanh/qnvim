---
name: qnvim-gui-testing
description: Den GUI-Modus testen – Protokolltest gegen echtes Neovim, Offscreen-Smoke-Test des Shell-Widgets mit Screenshot, und die manuelle Prüfliste in Qt Creator. Verwenden nach Änderungen an qnvim_gui.lua, der Session, den Startargumenten oder vor einem Release.
---

# GUI-Modus testen

## 1. Protokolltest (schnell, kein Qt, kein Qt Creator)

```sh
pip install pynvim
python3 tests/gui/test_protocol.py --nvim "$(command -v nvim)"
```

Startet Neovim wie das Plugin (`--embed`, Lua-Glue per `--cmd`) und spielt Qt
Creators Rolle über msgpack-rpc. Deckt ab: Start-`bufenter`, `add` ohne Umschalten,
`open` mit Zeile/Spalte (UTF-16 → Byte), Spezialfenster meiden, vorhandenes
Fenster anspringen, `modified`/`written`/Schreiben auf Platte, stilles `close`,
`bufdelete` bei `:bd`, entprellter `cursor`, Floats ohne `bufenter`, `:Qtc` +
Kurzbefehle + Vervollständigung.

Neuer Test: Methode in `GuiProtocolTest`; Aktion ausführen, dann
`ev = self.events()` (liest bis zu einem Sentinel) und Tupel prüfen.

## 2. Smoke-Test (echtes Shell-Widget, offscreen)

```sh
cmake -S . -B build-check -DQNVIM_BUILD_TESTS=ON -DQNVIM_TEST_NVIM="$(command -v nvim)" ...
cmake --build build-check --target qnvim_gui_smoke_run
# oder: tools/compilecheck/run.sh --tests
```

Prüft: UI hängt an, Shim geladen (`:GuiFont`), Lua-Glue geladen, `bufenter`
kommt an, Grid hat Inhalt und passende Größe, Completion-Menü wird von Neovim
gezeichnet. Screenshots: `build-check/tests/gui/smoke.png` und `smoke-pum.png`
– **ansehen** (Read-Tool), nicht nur PASS lesen: Syntaxfarben, Statuszeile,
Cmdline unten, Popup unter dem Cursor.

## 3. Manuelle Prüfliste in Qt Creator (vor Release)

Mit `QT_LOGGING_RULES='qtcreator.plugin.qnvim.*=true'` starten.

- [ ] Projekt öffnen, Datei aus dem Projektbaum → Neovim zeigt sie, Fokus in Neovim
- [ ] Locator (`:Locator`) → Datei:Zeile springt an die Zeile
- [ ] Compile-Fehler anklicken → richtige Zeile/Spalte
- [ ] Ändern → `*` im Tab/Dropdown; `Ctrl+S` in Neovim (`:w`) → `*` weg
- [ ] Ändern, `:Build` → Qt Creator speichert vorher (Datei auf Platte aktuell)
- [ ] `:e andere.cpp`, Telescope/fzf-lua → Qt Creators aktuelles Dokument folgt
- [ ] `:bd` → Dokument verschwindet aus „Offene Dokumente“
- [ ] Dokument in Qt Creator schließen (geändert, „Nicht speichern“) → Buffer weg
- [ ] Completion (nvim-cmp/blink), LSP-Hover, which-key, noice → sichtbar wie in nvim
- [ ] `:Qtc` ohne Argument → Picker; `:Qtc Debugger.<Tab>` vervollständigt
- [ ] `Ctrl+2`/`Alt+4` → Qt-Creator-Modus/Ausgabe (Durchreichung)
- [ ] `.ui`-Datei öffnet weiterhin im Designer
- [ ] „Öffnen mit“ → Qt-Creator-Texteditor geht noch
- [ ] `:qa` → Dokumente schließen, nächste Datei startet frisches Neovim
- [ ] Kaputte init.lua (`error()` am Anfang) → Meldung, keine Neustartschleife
- [ ] Qt Creator beenden mit geänderter Datei → Speichern-Dialog, Speichern schreibt
- [ ] Modus auf „Classic“ + Neustart → alte Texteditoren, keine Neovim-Zuordnungen mehr

## Ergebnis berichten

Welche Stufen gelaufen sind, PASS/FAIL je Prüfung, Pfad der Screenshots, und
was **nicht** geprüft werden konnte (z. B. kein Qt Creator verfügbar → Stufe 3 offen).
