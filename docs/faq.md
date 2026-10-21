# FAQ

## How does qnvim compare with Qt Creator's Vim mode?

qnvim runs a real Neovim instance and connects it to Qt Creator's editors. Your
Neovim configuration, plugins, Lua/VimL code and key mappings are available
directly in Qt Creator.

Qt Creator's built-in Vim mode is an emulation: it provides Vim-like
keybindings but not a full Neovim runtime or Neovim plugins.

Use qnvim if you want to keep your Neovim setup while working in Qt Creator.
Use Qt Creator's Vim mode if you want a lightweight, built-in keybinding layer
with no external dependency.

## Does qnvim run my `init.vim`/`init.lua`?

Yes. qnvim spawns the `nvim` found in `PATH` and lets it load your normal
configuration and plugins. See [Features](features.md#real-neovim).

## Do I need Neovim installed separately?

Yes. qnvim spawns the `nvim` executable from `PATH`; it does not bundle Neovim.

## Does qnvim use Neovim's colors and syntax highlighting?

Editor syntax highlighting comes from Qt Creator: by default, files opened from
Neovim use Qt Creator's text editor for their type, so they are highlighted like
files opened in Qt Creator. Types with no Qt Creator text editor open as plain
text.
Neovim's colorscheme and highlight groups are not used in editors; floating
windows use Neovim's colors and attributes. See
[Limitations](limitations.md#syntax-highlighting).

## Can I trigger Qt Creator actions from Vimscript?

Yes. Use `g:neovim_channel` and the `Gui` `triggerCommand` notification. See
[Configuration](configuration.md#triggering-qt-creator-commands).
