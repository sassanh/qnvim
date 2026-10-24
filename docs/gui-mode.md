# Neovim GUI mode

In GUI mode qnvim shows Neovim's own screen in Qt Creator's editor area, drawn
by [neovim-qt](https://github.com/equalsraf/neovim-qt)'s Shell widget — the
same widget `nvim-qt` uses. Everything Neovim draws appears exactly as in a
terminal or in nvim-qt:

- windows and splits, tabpages and the tabline, the statusline/winbar,
- the command line, messages and `:messages`, the `-- INSERT --` mode line,
- completion menus (built-in, nvim-cmp, blink.cmp), floating windows of LSP
  hovers, pickers (telescope, fzf-lua, snacks), which-key, noice, dashboards,
- the right-click `PopUp` menu and `:popup` menus,
- terminals (`:terminal`), help pages, file trees (neo-tree, nvim-tree, oil).

Your whole configuration runs unchanged; Neovim does not know it is inside an
IDE except for `g:QNVIM` and `g:QNVIM_GUI`, which are set to `1`.

The other mode, **Classic**, is the original qnvim: Neovim edits in the
background and Qt Creator's text editors draw the text. Choose the mode in
**Preferences > QNVim > General > Mode** and restart Qt Creator.

## How Qt Creator and Neovim work together

Neovim runs once; there is one Neovim screen. Qt Creator still gets one editor
per file buffer, so its document features keep working:

| Qt Creator                                      | Neovim                                              |
|-------------------------------------------------|-----------------------------------------------------|
| Open a file (project tree, locator, Open File)  | the file is shown in a normal window (`:drop`-like) |
| Jump to a line (compile output, search results, debugger, issues) | the cursor moves there                 |
| Open Documents list, document dropdown          | one entry per listed file buffer                    |
| Modified marker `*`, "Save All", save before build | follows `'modified'`; saving runs `:write` in Neovim |
| Close a document                                | `:bdelete!` (Qt Creator asked about unsaved changes) |
| Current document (VCS, "Build/Run current file")| Neovim's current buffer                             |
| Startup project changes                         | `:cd` to the project directory (optional)           |

Changes in Neovim go the other way: `:e`, a picker or `gd` into another file
makes that file Qt Creator's current document; `:bd`/`:bw` closes its document.
Floating and special windows (pickers, trees, quickfix, terminals) do not
change Qt Creator's document.

Neovim is drawn in the editor that is current. If you split Qt Creator's editor
area, the other split shows the last picture of Neovim; click it to move Neovim
there. Use Neovim's own `:split`/`:vsplit` for side-by-side editing.

## Keys

While Neovim has the keyboard focus, **every key goes to Neovim**, including
keys Qt Creator uses as shortcuts (`Ctrl+S`, `Ctrl+K`, `Ctrl+B`, `Esc`, ...).
The exceptions are listed in **Keys for Qt Creator** (default: `Ctrl+1`…`Ctrl+7`
for the modes, `Alt+0`…`Alt+9` for the sidebar and output panes). Click outside
the editor or use those keys to reach the rest of Qt Creator.

## Qt Creator commands from Neovim

| Command                 | Qt Creator action                                   |
|-------------------------|-----------------------------------------------------|
| `:Qtc {id}`             | any action by id, with completion (`:Qtc Proj<Tab>`) |
| `:Qtc`                  | pick an action with `vim.ui.select` (your telescope/fzf-lua/snacks ui-select) |
| `:Build`, `:BuildAll`, `:Rebuild`, `:RebuildAll`, `:Clean`, `:CleanAll`, `:Deploy` | build steps |
| `:Run`, `:Debug`, `:DebugContinue`, `:DebugStop`, `:DebugStepOver`, `:DebugStepInto`, `:DebugStepOut` | run and debug |
| `:Target`               | kit/target selector                                 |
| `:Locator`              | the locator (`Ctrl+K`)                              |
| `:QtcFind`              | advanced find                                       |

The short commands are only created when no command with that name exists, so
your own `:Build` wins. Map them as usual:

```lua
if vim.g.QNVIM_GUI == 1 then
  vim.keymap.set('n', '<leader>b', '<cmd>Build<cr>')
  vim.keymap.set('n', '<leader>r', '<cmd>Run<cr>')
  vim.keymap.set('n', '<leader>k', '<cmd>Locator<cr>')
  vim.keymap.set('n', '<leader>a', '<cmd>Qtc<cr>') -- pick any action
end
```

`<config>/qnvim.lua` or `<config>/qnvim.vim` (next to your init file) is sourced
after startup, like in classic mode.

## Font and GUI commands

The Shell starts with Qt Creator's text editor font (**Start with Qt Creator's
text editor font**). nvim-qt's GUI commands work: `:GuiFont Iosevka:h12`,
`:GuiPopupmenu 1` (Qt-drawn completion menu instead of Neovim's),
`:GuiTabline 1`, `:GuiLinespace 2`, `:GuiRenderLigatures 1`. Put them in
`ginit.vim`, which is sourced after the UI attached.

Note: nvim-qt stores the last `:GuiFont` and `:GuiPopupmenu`/`:GuiTabline`
choice in Qt Creator's settings file (keys `Gui/Font`, `ext_popupmenu`,
`ext_tabline`).

## Settings

**Preferences > QNVim > General**

| Setting                       | Default | Meaning |
|-------------------------------|---------|---------|
| Mode                          | Neovim GUI | GUI mode or classic mode (restart Qt Creator) |
| Neovim executable             | `nvim`  | name in `PATH` or absolute path |
| Additional arguments          | —       | e.g. `-u ~/.config/nvim-qtc/init.lua` or `--clean` |
| Keys for Qt Creator           | `Ctrl+1..7, Alt+0..9` | shortcuts that bypass Neovim |
| Keep Qt Creator's editor for  | Designer forms, SCXML, SVG | MIME types (and sub-types) not opened in Neovim |
| Open all text files in Neovim | on      | makes Neovim the default editor of every `text/plain` type |
| Start with Qt Creator's text editor font | on | |
| Change Neovim's directory to the startup project's directory | on | `:cd` on project change |

"Open all text files in Neovim" uses Qt Creator's own per-MIME-type default
editor setting (the one behind **Environment > MIME Types** and **Open With**),
so any file can still be opened in another editor with **Open With**, and a
default editor you picked yourself for a type is kept. Switching back to
classic mode removes the defaults qnvim set.

**Tools > QNVim** has **Show Neovim** (`Alt+Shift+V, Alt+Shift+V`; opens an
editor with Neovim's start screen if no file is open) and **Restart Neovim**.

## When Neovim exits

`:q` on the last window or `:qa` ends Neovim. Its documents are closed in Qt
Creator (Neovim already handled unsaved changes) and a fresh Neovim is started
for the next file. If Neovim dies within 3 seconds of starting (usually a
broken configuration), qnvim does not restart it; fix the configuration and
use **Restart Neovim**.

## Limitations of GUI mode

- Qt Creator features that need its own text editor are not available in a
  Neovim editor: the C++/QML code model's highlighting, refactoring actions,
  Qt Creator's completion and tooltips, the debugger's current-line marker and
  breakpoint gutter, inline diagnostics, Copilot. Use Neovim plugins (LSP with
  clangd/qmlls, nvim-dap) instead. Builds, runs, the debugger itself, locator,
  project tree, VCS and output panes work as usual.
- Neovim is drawn in one editor at a time (see above).
- Neovim's text is only in Neovim: Qt Creator's "Find in current document" and
  other features that read a document's text see nothing; "Find in files"
  reads the files on disk.
- `:GuiNewWindow` (opening another nvim-qt window) is not supported.
