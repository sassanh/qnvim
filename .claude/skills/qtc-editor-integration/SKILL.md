---
name: qtc-editor-integration
description: Die Qt-Creator-Seite des GUI-Modus ändern – NvimEditor/NvimDocument/NvimEditorHost/NvimEditorFactory, Shell-Widget einbetten und umhängen, MIME-Übernahme, Tastaturdurchreichung, Einstellungen, Menüaktionen. Verwenden bei Änderungen in src/gui/*.cpp|h, src/qnvimplugin.* oder src/qnvimsettings.*.
---

# GUI-Modus: Qt-Creator-Integration

## Klassen (src/gui/)

| Klasse | Rolle |
|---|---|
| `NvimGuiSession` | besitzt Connector, Shell, Factory, Dokumentregister; RPC; Neustart |
| `NvimEditorFactory` | Id `QNVim.NeovimGuiEditor` (stabil! steht in User-Settings), MIME `text/plain` |
| `NvimEditor` | 1 Editor pro Dokument (`setDuplicateSupported(false)`), besitzt Dokument und Host |
| `NvimDocument` | spiegelt Datei, `'modified'`, Cursor; `saveImpl` → `writeBuffer` (synchron) |
| `NvimEditorHost` | Widget im Editorbereich; hält die Shell oder zeigt einen Schnappschuss |
| `nvimGuiArguments()` | Startargumente, frei von Qt-Creator-Typen (vom Smoke-Test mitgenutzt) |

## Lebensdauer – die eine Regel

**Die Shell gehört nie einem Host.** Jeder Host gibt sie in `release()` frei
(auch im Destruktor), bevor er gelöscht wird; sonst löscht Qt sie als Kind mit.
`NvimEditor::~NvimEditor` ruft `m_host->release()` vor `delete m_host`.
Die Shell wird nur in `NvimGuiSession::shutdown()` und `onNeovimExited()`
gelöscht. Alle Zeiger auf Qt-Objekte über Grenzen hinweg sind `QPointer`.

## Shell umhängen

`onCurrentEditorChanged()` → `attachShellTo(host)`: alter Host `release()`
(macht Schnappschuss), neuer Host `adopt()` (setzt `focusProxy` → EditorManager
fokussiert automatisch die Shell). Andere Editortypen ändern nichts.
Neovim hat **einen** Bildschirm: in einer zweiten Qt-Creator-Split-Ansicht steht
nur der Schnappschuss.

## Neovim als Standardeditor

`takeOverTextEditors()` (in `QNVimPlugin::extensionsInitialized()`, also vor
Core!) schreibt für jeden MIME-Typ, der `text/plain` erbt und nicht
ausgeschlossen ist, die Editor-Id in `EditorManager/PreferredEditorFactories`.
Eigene Benutzerwahl für einen Typ bleibt erhalten. Die übernommenen Typen
stehen in `QNVim/Gui/TakenOverMimeTypes`; `releaseTextEditors()` (Klassik-Modus)
entfernt genau diese. Hintergrund: Skill `qtc-api-port`.

## Tasten

`eventFilter` auf der Shell akzeptiert jedes `ShortcutOverride` – außer
Sequenzen aus „Keys for Qt Creator“ (`m_passthrough`). Dadurch kommen
`Ctrl+S`, `Esc`, `Ctrl+K` … bei Neovim an. Tab/Backtab behandelt
`Shell::event()` selbst.

## Einstellungen hinzufügen

1. Aspect in `QNVimSettings` (`src/qnvimsettings.h`), `setSettingsKey("Gui/…")`,
   Label/Tooltip/Default im Konstruktor, ins Layout (Gruppe „Neovim GUI“).
2. Wirkung beim Start (`NvimGuiSession` liest `settings()`); Modus- und
   GUI-Optionen gelten nach Neustart (Hinweis-Label existiert).
3. Tabelle in `docs/gui-mode.md` ergänzen.

## Menüaktionen

In `QNVimPlugin::setupGuiMode()`: `ActionManager::registerAction(action, id,
Context(C_GLOBAL))`, ins Menü `Constants::MENU_ID` (Tools > QNVim). Ids in
`gui/nvimguiconstants.h`.

## Typische Fehler

- `EditorManager::openEditor` innerhalb einer Notification ohne
  `m_syncingFromNvim` → Ping-Pong.
- Lua-Aufruf vor dem UI-Attach → geht verloren, wenn nicht über `luaCall()`
  (Warteschlange) geschickt.
- Synchrones Warten (`writeBuffer`) nur mit `QEventLoop::ExcludeUserInputEvents`
  und Timeout – Neovim kann in einem Prompt hängen.
- Dokumentschlüssel: immer `key(path)` (kanonisch), nie roher String.
- Nach jeder Änderung: Skill `qnvim-build` (Compile-Check) und
  `qnvim-gui-testing`.
