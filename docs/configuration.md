# Configuration

## Command line, messages and completion menu

By default qnvim renders Neovim's command line, messages and completion menu.
To let a Neovim plugin such as [noice.nvim](https://github.com/folke/noice.nvim)
render them instead, open *Preferences > QNVim* (*Tools > Options > QNVim* on
Linux and Windows) and set **Command line, messages and completion menu** to
**Rendered by Neovim plugins**.

The choice is made when Neovim is attached, so restart qnvim after changing it
(toggle it off and on from the QNVim menu, or restart Qt Creator).

In plugin mode qnvim does not claim Neovim's message, command-line or
completion-menu widgets. A plugin must handle them; without one, they are not
shown. This option cannot be set from `qnvim.vim` or `qnvim.lua`: those files
are sourced after the UI is attached.

## Floating windows

qnvim places a floating window where Neovim anchors it: a float anchored to a
window (including one anchored to the cursor in that window) is shown in the
editor view that displays the window, and a float anchored to Neovim's screen
is shown at its screen cell in Qt Creator's main window.

## `qnvim.vim` and `qnvim.lua`

Put a `qnvim.vim` (or `qnvim.lua`) file in the same directory as your
`init.vim`/`init.lua`. qnvim sets `$MYQVIMRC` to that path (mind the `Q` after
`MY`) and sources it at startup. With `init.lua` it uses `qnvim.lua` when it
exists and falls back to `qnvim.vim`; with `init.vim` it uses `qnvim.vim` and
falls back to `qnvim.lua`.

The example configurations ([Vimscript](../examples/qnvim.vim),
[Lua](../examples/qnvim.lua)) contain ready-made mappings for building,
deploying, running, switching panes and triggering Qt Creator commands.

## Triggering Qt Creator commands

qnvim exposes its RPC channel as `g:neovim_channel`. Notify the `Gui` channel
with `triggerCommand` and a Qt Creator command id:

```vim
" Run the current project
nnoremap <silent> <leader>r :Run<cr>

" Open Qt Creator's options dialog
nnoremap <silent> <leader>o :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Options')<cr>
```

The command id is the one Qt Creator uses internally; an id that does not match
a registered command is ignored and logged. Examples:

- `ProjectExplorer.Build`
- `ProjectExplorer.Run`
- `TextEditor.AutoIndentSelection`
- `TextEditor.CompleteThis`
- `CppTools.SwitchHeaderSource`
- `QtCreator.Close`
- `QtCreator.Options`

You can pass several ids to one `triggerCommand` notification; qnvim triggers
them in order. This is useful for fallback commands:

```vim
nnoremap <silent> <c-]> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'TextEditor.FollowSymbolUnderCursor', 'TextEditor.JumpToFileUnderCursor')<cr>
```

## Ex commands

qnvim defines these commands in Neovim:

| Command | Qt Creator action |
| --- | --- |
| `:Build`, `:BuildProject` | Build the current project |
| `:BuildAll` | Build the session |
| `:Rebuild`, `:RebuildProject` | Rebuild the current project |
| `:RebuildAll` | Rebuild the session |
| `:Clean`, `:CleanProject` | Clean the current project |
| `:CleanAll` | Clean the session |
| `:Deploy`, `:DeployProject` | Deploy the current project |
| `:DeployAll` | Deploy the session |
| `:Run` | Run the current project |
| `:Debug`, `:DebugStart` | Start debugging |
| `:DebugContinue` | Continue debugging |
| `:QMake` | Run qmake |
| `:Target` | Select the target |

Commands can be combined with `|`, for example `:Build|Run`.

## Lua API

The same actions are available in Neovim's Lua through the `qnvim` module.
qnvim installs it before Neovim loads your configuration, so it can be required
from `init.lua`, `qnvim.vim`, `qnvim.lua` and plugins:

```lua
local qnvim = require('qnvim')

vim.keymap.set('n', '<leader>b', qnvim.build)
vim.keymap.set('n', '<leader>r', qnvim.run)
vim.keymap.set('n', '<leader>o', function()
  qnvim.trigger_command('QtCreator.Options')
end)
```

`qnvim.trigger_command(...)` triggers one or more Qt Creator command ids, in
order, like the `triggerCommand` notification above. The named functions are
shortcuts for the Ex commands:

| Function | Ex command |
| --- | --- |
| `qnvim.build()` | `:Build`, `:BuildProject` |
| `qnvim.build_all()` | `:BuildAll` (builds the session) |
| `qnvim.rebuild()` | `:Rebuild`, `:RebuildProject` |
| `qnvim.rebuild_all()` | `:RebuildAll` (rebuilds the session) |
| `qnvim.clean()` | `:Clean`, `:CleanProject` |
| `qnvim.clean_all()` | `:CleanAll` (cleans the session) |
| `qnvim.deploy()` | `:Deploy`, `:DeployProject` |
| `qnvim.deploy_all()` | `:DeployAll` (deploys the session) |
| `qnvim.run()` | `:Run` |
| `qnvim.debug()` | `:Debug`, `:DebugStart` |
| `qnvim.debug_continue()` | `:DebugContinue` |
| `qnvim.qmake()` | `:QMake` |
| `qnvim.target()` | `:Target` |

The functions send notifications to Qt Creator and need qnvim's RPC channel.
Neovim loads `init.lua` before qnvim attaches, so requiring the module there is
fine, but calling these functions at its top level raises an error. Key
mappings, `VimEnter` autocommands, `qnvim.vim` and `qnvim.lua` run late enough.
