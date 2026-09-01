# Changelog

All notable changes to this project will be documented in this file. This project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased](https://github.com/crow-translate/crow-translate/tree/HEAD)

[Full Changelog](https://github.com/crow-translate/crow-translate/compare/2.3.1...HEAD)

**Changed**

- Fix `number` not showing correctly when `relativenumber` was enabled.
- Remove cursor blinking.
- Fix incorrect plugin toggling.

## [1.2.0](https://github.com/sassanh/qnvim/tree/1.2.0) (2019-09-28)

[Full Changelog](https://github.com/sassanh/qnvim/compare/1.1.0...1.2.0)

**Changed**

- Zooming text editor (<kbd>Ctrl</kbd> + <kbd>Mouse Wheel</kbd>) updates block cursor width now.
- Fix relative number column.
- Zooming text editor doesn't ruin relative number column anymore.
- Fix toggle action not removing command-line and not showing native Qt Creator status line.
- Addressed all compilation warnings (except for diff_match_patch files).
- `:e` won't break editor state anymore.
- Support almost all buffer types (gina, fugitive, etc).
- Fix lots of small bugs.

## [1.1.0](https://github.com/sassanh/qnvim/tree/1.1.0) (2019-07-16)

[Full Changelog](https://github.com/sassanh/qnvim/compare/1.0.2...1.1.0)

**Changed**

- If `QNVIM_always_text` is set, it'll always open files opened by neovim with a text editor (avoid openning resource editor for example).
- Fix a segmentation fault that happened when `Open With` menu was used.
- Automatically run `cd` (change directory) in neovim when files changes in Qt Creator. It runs `cd` with the directory of the project (not the file).

## [1.0.2](https://github.com/sassanh/qnvim/tree/1.0.2) (2019-07-16)

[Full Changelog](https://github.com/sassanh/qnvim/compare/1.0.1...1.0.2)

**Changed**

- Fix a segmentation fault which happened after exiting some special terminal buffers (like fzf).
- Remove padding from patches in synching from neovim to Qt Creator, previously changes from neovim would make Qt Creator flicker colors around the change and would make Qt Creator spend CPU power to detect colors because it was rewriting texts around the patch (the patch had a padding) Now there should be no flickering, unnecessary CPU usage, etc when syncing from neovim to Qt Creator.

## [1.0.1](https://github.com/sassanh/qnvim/tree/1.0.1) (2019-07-15)

[Full Changelog](https://github.com/sassanh/qnvim/compare/1.0.0...1.0.1)

**Changed**

- Use `nvim_buf_set_lines` for synching from Qt Creator to neovim which should make synching from Qt Creator to neovim much more stable. (We still need patching for synching from neovim to Qt Creator because setting the whole buffer in Qt Creator each time and edit happens in neovim will be super slow.)
- Better handling status bar (showing multiline messages completely in multiple lines, avoiding scrollbar in any situation, etc).
- Fix status bar when showing mutliline messages would make status bar go blank forever.

## [1.0.0](https://github.com/sassanh/qnvim/tree/1.0.0) (2019-06-05)

[Full Changelog](https://github.com/sassanh/qnvim/compare/0.4.0...1.0.0)

**Changed**

- `$MYQVIMRC` is now a file named `qnvim.vim` in the same directory as `$MYVIMRC`.

## [0.4.0](https://github.com/sassanh/qnvim/tree/0.4.0) (2019-04-29)

**Added**

- `ext_messages` so now `echo` and its family are supported (considering we already had `ext_cmdline`, cmdline should be all supported).
- Tooltip for cmdline so that in case of big messages user can see the whole message by hovering the mouse over it).
