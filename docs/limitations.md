# Limitations

## Restarting and quitting Neovim

What `:restart` restores is what Neovim's session restores: buffers, splits,
window sizes and the cursor line as `:mksession` saves them. Neovim does not
save the cursor column or the exact scroll position (the top line) in a session,
so qnvim cannot bring those back.

After Neovim quits, qnvim has no backend. If Qt Creator's quit prompt is
cancelled, Qt Creator stays open without Neovim; toggle QNVim off and on
(*Tools > QNVim > Toggle QNVim*) to start a new one.

A Neovim that exits with a non-zero code is treated as a crash, not as a quit:
Qt Creator stays open and qnvim logs that Neovim exited.

## Tabpages

Neovim tabpages do not become Qt Creator tabs. Their windows appear as splits in
the same editor area, and the Neovim tabline is not shown.

## Split sizes

Neovim's `:resize`, `:vertical resize` and `<C-w>=` do not change Qt Creator's
split sizes. Drag the splitter to resize the views; Neovim's windows follow the
editor views.

## Unnamed buffers

Buffers without a file name (for example `:enew` or a plugin's scratch buffer)
do not get an editor view.

## Floating windows

Floating windows are display-only: the cursor is not drawn inside a float and
mouse events are not forwarded to it. Keys and clicks pass through to the editor
below, so use the keyboard with pickers and completion menus.

A float Neovim anchors to a window or to the cursor in a window is placed in
that window's editor view. A float anchored to another float (a completion
menu's scrollbar or its documentation window) follows that float's overlay. A
float anchored to Neovim's screen is placed at its screen cell, and qnvim's
screen is the whole main window. A plugin that takes an editor-relative
position instead of anchoring its float to a window gives qnvim no way to know
which view it belongs to, so the float stays at its screen cell unless a
`qnvim.float_filter()` filter places it in the editing area (see
[configuration](configuration.md)).

## Command line and messages handed to plugins

When the command line, messages and completion menu are set to be rendered by
Neovim plugins, qnvim does not render them itself. A plugin such as noice.nvim
must handle them; without one, the command line and messages are not visible.

## Syntax highlighting

Editor syntax highlighting comes from Qt Creator, not from Neovim's colorscheme
and highlight groups. By default, files opened from Neovim use Qt Creator's text
editor for their type, so they are highlighted like files opened in Qt Creator;
types with no Qt Creator text editor open as plain text. Floating windows use
Neovim's colors and attributes.

## Search highlighting

`'hlsearch'` matches are highlighted (see
[Features](features.md#search-highlighting)), but the other highlights Neovim
draws in a window are not: in particular, highlights added by plugins with
`matchadd()`/`matchaddpos()` (for example vim-sandwich's operator target) do not
appear. A search pattern that matches across lines is not highlighted either,
and at most 1000 matches are shown per buffer. While a `/` or `?` pattern is
being typed, all its matches are highlighted only when both `'incsearch'` and
`'hlsearch'` are on; with `'hlsearch'` off, the single match `'incsearch'`
highlights is not shown (the cursor still moves to it like in Neovim).
