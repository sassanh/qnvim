" qnvim's own Neovim setup (see QNVimCore::sendPreamble(), which sends this
" file as one multi-line `:command` through nvim_command). In that form a bare
" `autocmd ...` line absorbs the lines that follow it, so every autocmd must
" stay wrapped in `execute "autocmd ..."`. The caller sets g:neovim_channel.
let g:QNVIM_always_text=v:true
execute "command -bar Build call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'ProjectExplorer.Build')"
execute "command -bar BuildProject call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'ProjectExplorer.Build')"
execute "command -bar BuildAll call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'ProjectExplorer.BuildSession')"
execute "command -bar Rebuild call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'ProjectExplorer.Rebuild')"
execute "command -bar RebuildProject call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'ProjectExplorer.Rebuild')"
execute "command -bar RebuildAll call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'ProjectExplorer.RebuildSession')"
execute "command -bar Clean call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'ProjectExplorer.Clean')"
execute "command -bar CleanProject call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'ProjectExplorer.Clean')"
execute "command -bar CleanAll call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'ProjectExplorer.CleanSession')"
execute "command -bar Deploy call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'ProjectExplorer.Deploy')"
execute "command -bar DeployProject call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'ProjectExplorer.Deploy')"
execute "command -bar DeployAll call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'ProjectExplorer.DeploySession')"
execute "command -bar Run call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'ProjectExplorer.Run')"
execute "command -bar Debug call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'ProjectExplorer.Debug')"
execute "command -bar DebugStart call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'ProjectExplorer.Debug')"
execute "command -bar DebugContinue call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'Debugger.Continue')"
execute "command -bar QMake call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'Qt4Builder.RunQMake')"
execute "command -bar Target call rpcnotify(g:neovim_channel, 'Gui', 'triggerCommand', 'ProjectExplorer.SelectTargetQuick')"
execute "autocmd BufReadCmd * :call rpcnotify(g:neovim_channel, 'Gui', 'fileAutoCommand', 'BufReadCmd', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden, g:QNVIM_always_text)"
execute "autocmd BufReadCmd * if &buftype ==# 'help' | setlocal modifiable | silent! call setline(1, readfile(expand('<afile>:p'))) | setlocal nomodifiable nomodified | endif"
execute "autocmd TermOpen * :call rpcnotify(g:neovim_channel, 'Gui', 'fileAutoCommand', 'TermOpen', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden, g:QNVIM_always_text)"
execute "autocmd BufWriteCmd * :call rpcnotify(g:neovim_channel, 'Gui', 'fileAutoCommand', 'BufWriteCmd', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden, g:QNVIM_always_text)|set nomodified"
execute "autocmd BufEnter * nested :if win_gettype() ==# '' | call rpcnotify(g:neovim_channel, 'Gui', 'fileAutoCommand', 'BufEnter', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden, g:QNVIM_always_text, win_getid()) | endif"
execute "autocmd BufDelete * nested :call rpcnotify(g:neovim_channel, 'Gui', 'fileAutoCommand', 'BufDelete', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden, g:QNVIM_always_text)"
execute "autocmd BufHidden * nested :call rpcnotify(g:neovim_channel, 'Gui', 'fileAutoCommand', 'BufHidden', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden, g:QNVIM_always_text)"
execute "autocmd BufWipeout * nested :call rpcnotify(g:neovim_channel, 'Gui', 'fileAutoCommand', 'BufWipeout', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden, g:QNVIM_always_text)"
function! SetCursor(line, col)
    call cursor(a:line, a:col)
    if mode()[0] ==# 'i' || mode()[0] ==# 'R'
        execute "normal! i\<C-G>u\<C-c>"
    endif
    call cursor(a:line, a:col)
endfunction
function! QNVimWinEvent(event, win, buffer)
    if win_gettype(a:win) !=# ''
        return
    endif
    call rpcnotify(g:neovim_channel, 'Gui', 'winAutoCommand', a:event, a:win, a:buffer)
endfunction
execute "autocmd WinNew * call QNVimWinEvent('WinNew', win_getid(), winbufnr(win_getid()))"
execute "autocmd WinEnter * call QNVimWinEvent('WinEnter', win_getid(), winbufnr(win_getid()))"
execute "autocmd WinClosed * call QNVimWinEvent('WinClosed', str2nr(expand('<amatch>')), 0)"
function! QNVimWindowInfo()
    let l:windows = []
    for l:info in getwininfo()
        if win_gettype(l:info.winid) ==# ''
            let l:name = bufname(l:info.bufnr)
            call add(l:windows, [l:info.winid, l:info.bufnr, empty(l:name) ? '' : fnamemodify(l:name, ':p'), l:info.winrow, l:info.wincol, l:info.width, l:info.height, l:info.tabnr, l:info.winid ==# win_getid()])
        endif
    endfor
    return l:windows
endfunction
" Whether qnvim places the floating window in the editing area: the filter is a
" Lua function (qnvim.float_filter()), so qnvim asks it through this wrapper.
function! QNVimFloatMatches(win)
    return luaeval("require('qnvim')._matches_float(_A)", a:win)
endfunction
" Source the user's qnvim.vim/qnvim.lua: the file matching their Neovim
" configuration (init.lua -> qnvim.lua), falling back to the other extension.
function! QNVimSourceRc()
    if empty($MYVIMRC)
        return
    endif
    let l:dir = fnamemodify($MYVIMRC, ':p:h')
    if $MYVIMRC =~# 'init\.lua$'
        let l:rc = l:dir . '/qnvim.lua'
        let l:fallback = l:dir . '/qnvim.vim'
    else
        let l:rc = l:dir . '/qnvim.vim'
        let l:fallback = l:dir . '/qnvim.lua'
    endif
    if !filereadable(l:rc) && filereadable(l:fallback)
        let l:rc = l:fallback
    endif
    let $MYQVIMRC = l:rc
    if filereadable(l:rc)
        execute 'source' fnameescape(l:rc)
    endif
endfunction
execute "autocmd VimEnter * call QNVimSourceRc()"
