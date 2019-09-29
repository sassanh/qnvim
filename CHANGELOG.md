---

## 1

### 1.2.0    (2019-09-28)

- Zooming text editor (ctrl + mouse wheel) updates block cursor width now
- Relative number column fixed (It used to render weirdly)
- Zooming text editor doesn't ruin relative number column anymore
- Fixed toggle action not removing command-line and not showing native
    Qt Creator status line
- Addressed all compilation warnings (except for diff_match_patch files)
- `:e` won't break editor state anymore

### 1.1.0    (2019-07-16)

- If `QNVIM_always_text` is set, it'll always open files opened by neovim with
    a text editor (avoid openning resource editor for example)
- Fixed a segmentation fault that happened when `Open With` menu was used
- Automatically run `cd` (change directory) in neovim when files changes in
    Qt Creator. It runs `cd` with the directory of the project (not the file).

### 1.0.2    (2019-07-16)

- Fixed a segmentation fault which happened after exiting some special terminal
    buffers (like fzf)
- Removed padding from patches in synching from neovim to Qt Creator,
    previously changes from neovim would make Qt Creator flicker colors around
    the change and would make Qt Creator spend CPU power to detect colors
    because it was rewriting texts around the patch (the patch had a padding).
    Now there should be no flickering, unnecessary CPU usage, etc when syncing
    from neovim to Qt Creator.

### 1.0.1    (2019-07-15)

- Using `nvim_buf_set_lines` for synching from Qt Creator to neovim which should
    make synching from Qt Creator to neovim much more stable. (We still need
    patching for synching from neovim to Qt Creator because setting the whole
    buffer in Qt Creator each time and edit happens in neovim will be super
    slow.)
- Better handling status bar (showing multiline messages completely in
    multiple lines, avoiding scrollbar in any situation, etc)
- Fixed a bug in status bar when showing mutliline messages would make status
    bar go blank forever


### 1.0.0    (2019-06-05)

- $MYQVIMRC is now a file named `qnvim.vim` in the same directory as $MYVIMRC


---

## 0

### 0.4.0    (2019-04-29)

- Started writing `CHANGELOG.md`
- Added `ext_messages` so now `echo` and its family are supported (considering
    we already had `ext_cmdline`, cmdline should be all supported)
- Added tooltip for cmdline so that in case of big messages user can see the
    whole message by hovering the mouse over it


---
