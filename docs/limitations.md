# Limitations

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
which view it belongs to, so the float stays at its screen cell.

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
