---
name: nvim-gui-protocol
description: Understand and extend the RPC protocol between Neovim (src/gui/qnvim_gui.lua) and Qt Creator (src/gui/nvimguisession.cpp) in GUI mode — new events, new Lua functions, new :Qtc commands, startup order. Use for any change to the synchronization between Neovim buffers and Qt Creator documents.
---

# GUI mode: Neovim ⇄ Qt Creator protocol

## Architecture in one sentence

One Neovim (`nvim --embed`), drawn by neovim-qt's `NeovimQt::Shell` in the
currently active `NvimEditor`; one lightweight `NvimDocument` per listed file
buffer in Qt Creator, mirroring only the file name, `'modified'`, and cursor.

## Direction Neovim → Qt Creator

`rpcnotify(g:qnvim_gui_channel, 'QNVimGui', <event>, ...)`, handled in
`NvimGuiSession::onNotification()`:

| Event | Arguments | Qt Creator reaction |
|---|---|---|
| `bufenter` | path | open/activate the document (`openEditor(path, EDITOR_ID)`) with `m_syncingFromNvim` |
| `bufdelete` | path | close the document, `closingFromNvim = true` |
| `modified` | path, bool | `NvimDocument::setModifiedFromNvim()` |
| `written` | path | modified = false |
| `cursor` | path, line, utf16col | `currentLine()/currentColumn()` (debounced 150 ms) |
| `action` | id | `ActionManager::command(id)->action()->trigger()` (deferred) |
| `actions` | – | send the action list again |

Only **listed normal file buffers** are reported (`buftype == ''`, `buflisted`,
name without `scheme://`). Floats do not trigger `bufenter`.

## Direction Qt Creator → Neovim

`luaeval(code, _A)` via `NvimGuiSession::luaCall()`. A single argument is passed
as `_A`; multiple arguments become the list `_A[1], _A[2], …`.

| Function | Purpose |
|---|---|
| `QNVimGui.open(path, line, col, center)` | show the file; prefer an existing window, avoid special windows (floats, trees, quickfix, terminal, `winfixbuf`); `col` is UTF-16 (Qt) and is converted to bytes |
| `QNVimGui.add(path)` | create and list the buffer without switching |
| `QNVimGui.close(path)` | `nvim_buf_delete(force)`; suppresses its own `bufdelete` |
| `QNVimGui.write(path)` | synchronous (QEventLoop, 10 s) — returns `''` or an error message |
| `QNVimGui.set_actions(list)` | `[[id, description], …]` for `:Qtc` completion |

## Startup order (do not change without reason)

1. `NeovimConnector::spawn(nvimGuiArguments(...))` — Lua glue via
   `--cmd lua dofile(...)` (before init.lua, independent of `'runtimepath'`,
   which lazy.nvim resets), shim via `--cmd source` **and** on `rtp`.
2. `ready` → `nvim_set_var('qnvim_gui_channel', channel)`. With `--embed`,
   Neovim waits with startup until `ui_attach`, so the variable is set before
   `VimEnter`.
3. Shell becomes visible → `ui_attach` → `neovimAttachmentChanged(true)` →
   `m_ready`, action list, `:cd`, then the queued `m_pending` calls.
   **Before attach, `QNVimGui` does not exist yet** — hence the queue.

## Avoid loops

- Qt Creator → Neovim → `bufenter` → Qt Creator: `m_nvimCurrentFile` is set
  before the Lua call; `onBufEnter()` does nothing if the current editor already
  shows the file.
- Neovim → Qt Creator → `currentEditorChanged` → Neovim: `m_syncingFromNvim`.
- Closing: `closingFromNvim` (Neovim has already deleted) or `M._closing` in Lua
  (Qt Creator has deleted).

## Adding a new event — procedure

1. Lua: autocmd in `qnvim_gui.lua` with `notify('<event>', ...)`; always pass
   paths through `file_path(buf)`/`normalize()`.
2. C++: branch in `onNotification()`; get the document via `documentFor(path)`
   (canonical paths, symlinks!).
3. Test in `tests/gui/test_protocol.py` (pattern: trigger an action, read
   `self.events()`, check the tuple).
4. Docs: table in `docs/gui-mode.md` if user-visible.

## New user command

Shortcuts live in the `shortcuts` table in `qnvim_gui.lua` (name → Qt Creator
action id). They are only created if the name is free. Find ids with
`:Qtc <Tab>` or in the Qt Creator sources
(`grep -rn 'registerAction(.*"Debugger\.' src/plugins`).

## Pitfalls

- `luaeval` with `nil`: `luaCall("…", {QVariant()})` passes `nil`.
- `vim.str_byteindex`/`str_utfindex` have different signatures in 0.10 and 0.11 —
  the helpers try both.
- `CursorMoved` does not fire on API calls without a redraw; in tests use
  `doautocmd CursorMoved`.
- Classic mode uses `rpcnotify(..., 'Gui', ...)` — do not mix them; the GUI
  mode's method name is `QNVimGui` (`Constants::RPC_METHOD`). In GUI mode,
  `'Gui'` belongs to the neovim-qt Shell.
