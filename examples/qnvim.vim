" SPDX-FileCopyrightText: 2018-2019 Sassan Haradji <sassanh@gmail.com>
" SPDX-License-Identifier: CC0-1.0

nnoremap <d-b> :Build<cr>
nnoremap <d-d> :Deploy<cr>
nnoremap <d-r> :Run<cr>
nnoremap <d-t> :Target<cr>

command! B :Build
command! BD :Build|Deploy
command! BR :Build|Run
command! BDR :Build|Deploy|Run
command! D :Deploy
command! DR :Deploy|Run
command! R :Run
command! Q :QMake
nnoremap <silent> qq :Q<cr>
nnoremap <silent> 1q :B<cr>
nnoremap <silent> 2q :D<cr>
nnoremap <silent> 3q :BD<cr>
nnoremap <silent> 4q :R<cr>
nnoremap <silent> 5q :BR<cr>
nnoremap <silent> 6q :DR<cr>
nnoremap <silent> 7q :BDR<cr>


nnoremap <silent> == :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'TextEditor.AutoIndentSelection')<cr>
vnoremap <silent> = :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'TextEditor.AutoIndentSelection')<cr>
nnoremap <silent> <c-]> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'TextEditor.FollowSymbolUnderCursor', 'TextEditor.JumpToFileUnderCursor')<cr>
nnoremap <silent> <f4> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'CppTools.SwitchHeaderSource')<cr>
nnoremap <silent> <d-w> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Close', 'TextEditor.JumpToFileUnderCursor')<cr>
nnoremap <silent> <c-tab> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.GotoPreviousInHistory')<cr>
nnoremap <silent> <c-s-tab> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.GotoPreviousInHistory')<cr>
inoremap <silent> <expr> <c-space> rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'TextEditor.CompleteThis') ? '<c-o><esc>' : '<c-o><esc>'

nnoremap <silent> <d-1> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Pane.Issues')<cr>
nnoremap <silent> <d-2> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Pane.SearchResults')<cr>
nnoremap <silent> <d-3> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Pane.ApplicationOutput')<cr>
nnoremap <silent> <d-4> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Pane.CompileOutput')<cr>
nnoremap <silent> <d-5> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Pane.DebuggerConsole')<cr>
nnoremap <silent> <d-6> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Pane.To-DoEntries')<cr>
nnoremap <silent> <d-7> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Pane.GeneralMessages')<cr>
nnoremap <silent> <d-8> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Pane.VersionControl')<cr>

nnoremap <silent> <c-1> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Mode.Welcome')<cr>
nnoremap <silent> <c-2> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Mode.Edit')<cr>
nnoremap <silent> <c-3> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Mode.Design')<cr>
nnoremap <silent> <c-4> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Mode.Mode.Debug')<cr>
nnoremap <silent> <c-5> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Mode.Project')<cr>
nnoremap <silent> <c-6> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Mode.Help')<cr>
nnoremap <silent> <c-7> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Mode.Welcome')<cr>

nnoremap <silent> <f1> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'Help.Context')<cr>

nnoremap <silent> <d-0> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.ToggleLeftSidebar')<cr>
nnoremap <silent> <d-a-0> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.ToggleModeSelector')<cr>
nnoremap <silent> <d-s-0> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.ToggleRightSidebar')<cr>
nnoremap <silent> <d-s-F> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.ToggleFullScreen')<cr>
nnoremap <silent> <d-t> :Target<cr>

nnoremap <silent> <a-cr> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'TextEditor.QuickFix')<cr>
nnoremap <silent> <d-,> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QtCreator.Options')<cr>

nnoremap <silent> <a-V><a-V> :call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'QNVim.Toggle')<cr>

unmap <tab>

set norelativenumber
