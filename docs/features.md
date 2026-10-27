# Features

qnvim attaches to Neovim as a UI. Neovim remains the source of truth for
buffers, windows, the cursor, the command line and messages. Qt Creator's editor
widgets render the text and provide the IDE: building, running, debugging, code
navigation and the rest of Qt Creator.

## Real Neovim

qnvim spawns the `nvim` executable found in `PATH` and runs your
`init.vim`/`init.lua`, plugins and key mappings. Neovim is not emulated.

## Buffers and synchronization

- Buffer contents, cursor position and modified state are kept in sync between
  Neovim and Qt Creator's editors. Selections made in Qt Creator are sent to
  Neovim.
- Files opened in Qt Creator become Neovim buffers. Buffers opened in Neovim get
  editors in Qt Creator.
- Two editors showing the same document share one Neovim buffer. Closing one of
  them does not delete the buffer; the remaining editor takes it over.
- Closing a buffer in Neovim (`:bd`, `:bw`, ...) closes its editors without
  asking Neovim to delete the buffer again, so `E516` does not appear.
- File names containing spaces, quotes or backslashes are escaped before being
  opened in Neovim.
- Filetype detection runs for files opened in Qt Creator, so filetype options
  such as `'commentstring'` are set.

## Windows and splits

- Neovim's `:split`, `:vsplit` and other windows appear as Qt Creator splits in
  the same arrangement, including the side of the source window.
- Qt Creator's own splits become Neovim windows.
- Each window keeps its own buffer, cursor and viewport. Moving between Neovim
  windows activates the matching split; activating a split makes its window the
  current one in Neovim.
- Each window is sized to the editor view that shows it.
- Closing a Neovim window removes its Qt Creator split. Closing a split, or the
  last editor in it, closes its Neovim window. Neovim's last window is left
  open, since Neovim cannot close it.

## Floating windows

- Completion menus, hover documentation, `fzf-lua`/Telescope pickers, the
  dashboard and other floating windows are shown as overlays.
- Each overlay uses the text, colors, attributes and stacking order Neovim
  reports for the float.
- A float anchored to Neovim's screen is placed at its screen cell in Qt
  Creator's main window (pickers, the dashboard, notifications, which-key,
  ...).
- A float Neovim anchors to a window or to the cursor in a window is placed in
  the editor view that shows that window, so a completion menu or hover popup
  stays next to the text it belongs to even though Qt Creator's editor layout
  differs from Neovim's window layout.
- A float anchored to another float (a completion menu's scrollbar or its
  documentation window) follows that float's overlay.

## Command line and messages

- Neovim's command line and message area are rendered at the bottom of the
  editor view, like Neovim's own command line, and drawn above Qt Creator's
  status bar instead of behind it.
- Long commands and multiline messages are shown in full and can be scrolled.
- Messages are cleared and hidden when Neovim clears them; the command line is
  hidden when Neovim hides it.
- The command line, messages and completion menu can be handed over to Neovim
  plugins such as noice.nvim; see [Configuration](configuration.md).

## Viewport and scrolling

Commands that scroll Neovim's window without moving the cursor (`zz`, `zt`,
`zb`, `<C-e>`, `<C-y>`, `<C-d>`, `<C-u>`, `H`, `M`, `L`, ...) also scroll the
editor view to Neovim's top line.

## Line numbers

Absolute and relative line numbers are drawn on the editor's own line geometry,
so they stay aligned with wrapped lines, folds and the partially visible top
line.

## Wrapping

- Neovim's `'wrap'` option and Qt Creator's *Enable text wrapping* setting are
  kept in sync in both directions.
- Each window's text area is kept as wide as the editor view that shows it, so
  Neovim wraps lines where the editor wraps them and `gj`/`gk` land on the row
  you see.
- The editor's cursor is kept on Neovim's exact position.

## Special buffers

Help pages (`:help`) and terminal buffers are shown in editor views.

## Project directory

Neovim's working directory follows the project of the file you open in Qt
Creator.

## Toggle

*Tools > QNVim > Toggle QNVim* enables or disables qnvim at runtime. Its default
shortcut is <kbd>Alt</kbd>+<kbd>Shift</kbd>+<kbd>V</kbd> pressed twice.

## IDE commands

qnvim defines `:Build`, `:Rebuild`, `:Clean`, `:Deploy`, `:Run`, `:Debug`,
`:QMake`, `:Target` and related commands, and a Lua API with the same actions
through `require('qnvim')`. Any registered Qt Creator command can also be
triggered by id through the RPC channel. See
[Configuration](configuration.md#triggering-qt-creator-commands).
