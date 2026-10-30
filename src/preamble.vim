" qnvim's own Neovim setup (see QNVimCore::sendPreamble(), which sends this
" file as one multi-line `:command` through nvim_command). In that form a bare
" `autocmd ...` line absorbs the lines that follow it, so every autocmd must
" stay wrapped in `execute "autocmd ..."`. A leading `\` is not joined to the
" line before it either: it stays in the function body and fails when the
" function is called. The caller sets g:neovim_channel.
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
" Whether a search pattern is matched case-sensitively because of 'smartcase':
" an upper case character in the pattern (not one that is part of an escape
" such as \V or \W) makes a search case-sensitive while 'ignorecase' is on.
function! QNVimPatternHasUpper(pattern)
    let l:chars = split(a:pattern, '\zs')
    let l:i = 0
    while l:i < len(l:chars)
        let l:char = l:chars[l:i]
        if l:char ==# '\'
            let l:i += 2
            continue
        endif
        if l:char =~# '[[:upper:]]'
            return 1
        endif
        let l:i += 1
    endwhile
    return 0
endfunction
" The matches of {pattern} in each of the loaded buffers {buffers}, as
" [buffer, [[line, start column, end column], ...]] entries. Lines are 1-based
" and columns are 1-based UTF-8 byte columns, the end column being exclusive.
" At most 1000 matches are collected per buffer, so that a pattern that matches
" everywhere in a big buffer does not make the answer huge.
function! QNVimSearchMatches(pattern, buffers)
    let l:result = []
    if empty(a:pattern) || !exists('*matchbufline')
        return l:result
    endif

    " matchbufline() computes its matches like a search, except that it does
    " not apply 'smartcase'. Prefixing the pattern with \C makes the matches
    " case-sensitive, like the search it comes from; a \c in the pattern comes
    " later and still overrides it.
    let l:pattern = a:pattern
    if &ignorecase && &smartcase && QNVimPatternHasUpper(l:pattern)
        let l:pattern = '\C' . l:pattern
    endif
    try
        for l:buffer in a:buffers
            if !bufexists(l:buffer) || !bufloaded(l:buffer)
                continue
            endif
            let l:linecount = getbufinfo(l:buffer)[0].linecount
            let l:matches = []
            let l:first = 1
            " Search in chunks, so the scan stops soon after the 1000th match
            " instead of walking the rest of a huge buffer.
            while l:first <= l:linecount && len(l:matches) < 1000
                let l:last = min([l:first + 499, l:linecount])
                for l:match in matchbufline(l:buffer, l:pattern, l:first, l:last)
                    if len(l:matches) >= 1000
                        break
                    endif
                    call add(l:matches, [l:match.lnum, l:match.byteidx + 1, l:match.byteidx + strlen(l:match.text) + 1])
                endfor
                let l:first = l:last + 1
            endwhile
            call add(l:result, [l:buffer, l:matches])
        endfor
    catch
        " A pattern that is not valid (yet) matches nothing.
    endtry
    return l:result
endfunction
" The search pattern being typed in the / or ? command line {line}: the text
" before a trailing search offset, when that is what the text after the last
" unescaped delimiter looks like. An offset such as /foo/e or /foo/+2 decides
" where the cursor lands, not which text matches, so it is left out.
function! QNVimCmdlinePattern(type, line)
    let l:delimiter = a:type ==# '?' ? '?' : '/'
    let l:last = -1
    let l:escaped = 0
    let l:i = 0
    while l:i < len(a:line)
        let l:char = a:line[l:i]
        if l:escaped
            let l:escaped = 0
        elseif l:char ==# '\'
            let l:escaped = 1
        elseif l:char ==# l:delimiter
            let l:last = l:i
        endif
        let l:i += 1
    endwhile
    if l:last < 0
        return a:line
    endif
    let l:offset = strpart(a:line, l:last + 1)
    if l:offset !~# '^\%([esb][+-]\?\d*\|[+-]\?\d*\|;.*\)\?$'
        return a:line
    endif
    return strpart(a:line, 0, l:last)
endfunction
" While a / or ? search is typed, report its pattern so that its matches can be
" highlighted along with 'incsearch' and 'hlsearch'.
execute "autocmd CmdlineChanged [/?] call rpcnotify(g:neovim_channel, 'Gui', 'searchPreview', (&incsearch && &hlsearch) ? QNVimCmdlinePattern(getcmdtype(), getcmdline()) : '')"
execute "autocmd CmdlineLeave [/?] call rpcnotify(g:neovim_channel, 'Gui', 'searchPreview', '')"
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
