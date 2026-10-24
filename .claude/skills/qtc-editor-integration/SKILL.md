---
name: qtc-editor-integration
description: Change the Qt Creator side of GUI mode — NvimEditor/NvimDocument/NvimEditorHost/NvimEditorFactory, embedding and re-parenting the Shell widget, MIME takeover, keyboard pass-through, settings, menu actions. Use for changes in src/gui/*.cpp|h, src/qnvimplugin.* or src/qnvimsettings.*.
---

# GUI mode: Qt Creator integration

## Classes (src/gui/)

| Class | Role |
|---|---|
| `NvimGuiSession` | owns connector, Shell, factory, document registry; RPC; restart |
| `NvimEditorFactory` | id `QNVim.NeovimGuiEditor` (stable! stored in user settings), MIME `text/plain` |
| `NvimEditor` | one editor per document (`setDuplicateSupported(false)`), owns document and host |
| `NvimDocument` | mirrors file, `'modified'`, cursor; `saveImpl` → `writeBuffer` (synchronous) |
| `NvimEditorHost` | widget in the editor area; holds the Shell or shows a snapshot |
| `nvimGuiArguments()` | startup arguments, free of Qt Creator types (also used by the smoke test) |

## Lifetime — the one rule

**The Shell never belongs to a host.** Every host releases it in `release()`
(also in the destructor) before it is deleted; otherwise Qt deletes it as a
child. `NvimEditor::~NvimEditor` calls `m_host->release()` before
`delete m_host`. The Shell is deleted only in `NvimGuiSession::shutdown()` and
`onNeovimExited()`. All pointers to Qt objects across boundaries are `QPointer`.

## Re-parenting the Shell

`onCurrentEditorChanged()` → `attachShellTo(host)`: old host `release()` (takes a
snapshot), new host `adopt()` (sets `focusProxy` → EditorManager focuses the
Shell automatically). Other editor types change nothing. Neovim has **one**
screen: a second Qt Creator split view shows only the snapshot.

## Neovim as the default editor

`takeOverTextEditors()` (in `QNVimPlugin::extensionsInitialized()`, i.e. before
Core!) writes the editor id into `EditorManager/PreferredEditorFactories` for
every MIME type that inherits `text/plain` and is not excluded. A user's own
choice for a type is preserved. The taken-over types are in
`QNVim/Gui/TakenOverMimeTypes`; `releaseTextEditors()` (classic mode) removes
exactly those. Background: skill `qtc-api-port`.

## Keys

`eventFilter` on the Shell accepts every `ShortcutOverride` — except sequences
from "Keys for Qt Creator" (`m_passthrough`). This lets `Ctrl+S`, `Esc`,
`Ctrl+K` … reach Neovim. Tab/Backtab are handled by `Shell::event()` itself.

## Adding settings

1. Aspect in `QNVimSettings` (`src/qnvimsettings.h`), `setSettingsKey("Gui/…")`,
   label/tooltip/default in the constructor, add it to the layout (group
   "Neovim GUI").
2. Effect at startup (`NvimGuiSession` reads `settings()`); mode and GUI options
   take effect after a restart (a notice label exists).
3. Add the setting to the table in `docs/gui-mode.md`.

## Menu actions

In `QNVimPlugin::setupGuiMode()`: `ActionManager::registerAction(action, id,
Context(C_GLOBAL))`, add it to the menu `Constants::MENU_ID` (Tools > QNVim).
Ids are in `gui/nvimguiconstants.h`.

## Typical mistakes

- `EditorManager::openEditor` inside a notification without
  `m_syncingFromNvim` → ping-pong.
- Lua call before UI attach → lost unless sent through `luaCall()` (queue).
- Synchronous waiting (`writeBuffer`) only with `QEventLoop::ExcludeUserInputEvents`
  and a timeout — Neovim can be stuck in a prompt.
- Document keys: always `key(path)` (canonical), never a raw string.
- After every change: skill `qnvim-build` (compile check) and
  `qnvim-gui-testing`.
