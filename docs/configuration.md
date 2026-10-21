# Configuration

## `qnvim.vim`

Put a `qnvim.vim` file in the same directory as your `init.vim`/`init.lua`.
qnvim sets `$MYQVIMRC` to that path (mind the `Q` after `MY`) and sources it at
startup.

The [example configuration](../examples/qnvim.vim) contains ready-made mappings
for building, deploying, running, switching panes and triggering Qt Creator
commands.

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
