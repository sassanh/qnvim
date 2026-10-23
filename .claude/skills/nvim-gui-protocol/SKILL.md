---
name: nvim-gui-protocol
description: Das RPC-Protokoll zwischen Neovim (src/gui/qnvim_gui.lua) und Qt Creator (src/gui/nvimguisession.cpp) im GUI-Modus verstehen und erweitern – neue Ereignisse, neue Lua-Funktionen, neue :Qtc-Befehle, Startreihenfolge. Verwenden bei jeder Änderung an der Synchronisation zwischen Neovim-Buffern und Qt-Creator-Dokumenten.
---

# GUI-Modus: Protokoll Neovim ⇄ Qt Creator

## Architektur in einem Satz

Ein Neovim (`nvim --embed`), gezeichnet von neovim-qt's `NeovimQt::Shell` im
gerade aktiven `NvimEditor`; pro gelistetem Datei-Buffer ein leichtgewichtiges
`NvimDocument` in Qt Creator, das nur Dateiname, `'modified'` und Cursor spiegelt.

## Richtung Neovim → Qt Creator

`rpcnotify(g:qnvim_gui_channel, 'QNVimGui', <event>, ...)`, verarbeitet in
`NvimGuiSession::onNotification()`:

| Event | Argumente | Qt-Creator-Reaktion |
|---|---|---|
| `bufenter` | path | Dokument öffnen/aktivieren (`openEditor(path, EDITOR_ID)`) mit `m_syncingFromNvim` |
| `bufdelete` | path | Dokument schließen, `closingFromNvim = true` |
| `modified` | path, bool | `NvimDocument::setModifiedFromNvim()` |
| `written` | path | modified = false |
| `cursor` | path, line, utf16col | `currentLine()/currentColumn()` (entprellt 150 ms) |
| `action` | id | `ActionManager::command(id)->action()->trigger()` (verzögert) |
| `actions` | – | Aktionsliste neu senden |

Gemeldet werden nur **gelistete normale Datei-Buffer** (`buftype == ''`,
`buflisted`, Name ohne `scheme://`). Floats lösen kein `bufenter` aus.

## Richtung Qt Creator → Neovim

`luaeval(code, _A)` über `NvimGuiSession::luaCall()`. Ein Argument wird direkt
`_A`, mehrere werden zur Liste `_A[1], _A[2], …`.

| Funktion | Zweck |
|---|---|
| `QNVimGui.open(path, line, col, center)` | Datei zeigen; vorhandenes Fenster bevorzugt, Spezialfenster (Floats, Trees, quickfix, terminal, `winfixbuf`) meiden; `col` ist UTF-16 (Qt) und wird in Bytes umgerechnet |
| `QNVimGui.add(path)` | Buffer anlegen und listen, ohne umzuschalten |
| `QNVimGui.close(path)` | `nvim_buf_delete(force)`; unterdrückt das eigene `bufdelete` |
| `QNVimGui.write(path)` | synchron (QEventLoop, 10 s) – Rückgabe `''` oder Fehlertext |
| `QNVimGui.set_actions(list)` | `[[id, beschreibung], …]` für `:Qtc`-Vervollständigung |

## Startreihenfolge (nicht verändern ohne Grund)

1. `NeovimConnector::spawn(nvimGuiArguments(...))` – Lua-Glue per
   `--cmd lua dofile(...)` (vor init.lua, unabhängig von `'runtimepath'`, das
   lazy.nvim zurücksetzt), Shim per `--cmd source` **und** auf `rtp`.
2. `ready` → `nvim_set_var('qnvim_gui_channel', channel)`. Mit `--embed` wartet
   Neovim mit dem Startup bis zum `ui_attach`, die Variable ist also vor
   `VimEnter` gesetzt.
3. Shell wird sichtbar → `ui_attach` → `neovimAttachmentChanged(true)` →
   `m_ready`, Aktionsliste, `:cd`, dann die gesammelten `m_pending`-Aufrufe.
   **Vor dem Attach existiert `QNVimGui` noch nicht** – deshalb Warteschlange.

## Schleifen vermeiden

- Qt Creator → Neovim → `bufenter` → Qt Creator: `m_nvimCurrentFile` wird vor
  dem Lua-Aufruf gesetzt; `onBufEnter()` tut nichts, wenn der aktuelle Editor
  schon die Datei zeigt.
- Neovim → Qt Creator → `currentEditorChanged` → Neovim: `m_syncingFromNvim`.
- Schließen: `closingFromNvim` (Neovim hat schon gelöscht) bzw. `M._closing`
  in Lua (Qt Creator hat gelöscht).

## Neues Ereignis hinzufügen – Ablauf

1. Lua: Autocmd in `qnvim_gui.lua` mit `notify('<event>', ...)`; Pfade immer
   über `file_path(buf)`/`normalize()`.
2. C++: Zweig in `onNotification()`; Dokument über `documentFor(path)` (kanonische
   Pfade, Symlinks!).
3. Test in `tests/gui/test_protocol.py` (Muster: Aktion auslösen,
   `self.events()` lesen, Tupel prüfen).
4. Doku: Tabelle in `docs/gui-mode.md` falls benutzersichtbar.

## Neuer Benutzerbefehl

Kurzbefehle stehen in der Tabelle `shortcuts` in `qnvim_gui.lua` (Name →
Qt-Creator-Action-Id). Sie werden nur angelegt, wenn der Name frei ist. Ids
findet man mit `:Qtc <Tab>` oder in den Qt-Creator-Quellen
(`grep -rn 'registerAction(.*"Debugger\.' src/plugins`).

## Fallstricke

- `luaeval` mit `nil`: `luaCall("…", {QVariant()})` übergibt `nil`.
- `vim.str_byteindex`/`str_utfindex` haben in 0.10 und 0.11 andere Signaturen –
  die Helfer probieren beide.
- `CursorMoved` feuert bei API-Aufrufen nicht ohne Redraw; im Test
  `doautocmd CursorMoved`.
- Klassischer Modus nutzt `rpcnotify(..., 'Gui', ...)` – nicht vermischen; der
  Methodenname des GUI-Modus ist `QNVimGui` (`Constants::RPC_METHOD`). `'Gui'`
  gehört im GUI-Modus der neovim-qt-Shell.
