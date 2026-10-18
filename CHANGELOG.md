# Changelog

All notable changes to this project will be documented in this file. This project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased](https://github.com/sassanh/qnvim/compare/v20.0.2...HEAD)

[Full Changelog](https://github.com/sassanh/qnvim/compare/v20.0.2...HEAD)

**Fixed**

- Sync the editor viewport with Neovim's window: commands that scroll Neovim's window without moving the cursor (`zz`, `zt`, `zb`, `<C-e>`, `<C-y>`, `<C-d>`, `<C-u>`, `H`, `M`, `L`, ...) used to leave the Qt Creator editor view where it was, so they appeared to do nothing. The editor is now scrolled to Neovim's top line ([#15](https://github.com/sassanh/qnvim/issues/15)).

## [20.0.2](https://github.com/sassanh/qnvim/tree/v20.0.2) (2026-10-18)

[Full Changelog](https://github.com/sassanh/qnvim/compare/v20.0.1...v20.0.2)

**Fixed**

- Fix a Qt Creator crash when Neovim is not running: opening a file or typing used to send requests over a connection that was not ready yet (dereferencing a null `NeovimApi2`). The connection's readiness is now checked before every request and connection errors are logged instead.
- Fix filetype detection never running for files opened in Qt Creator, which left `'filetype'` and filetype-plugin options such as `'commentstring'` empty for all filetypes ([#64](https://github.com/sassanh/qnvim/issues/64)).
- Fix macOS compilation failure caused by the SDK's `usr/include` leaking into the include path through the `Cups::Cups`/`ZLIB::ZLIB` imported targets (made `<cmath>` resolve to the C header instead of libc++'s wrapper).

**Changed**

- Document that on macOS the plugin binds to the Qt bundled in Qt Creator when it loads, and add `tools/RetargetPluginQt.sh` to retarget a plugin built against a Qt that links its frameworks by absolute path.

## [20.0.1](https://github.com/sassanh/qnvim/tree/v20.0.1) (2026-09-02)

[Full Changelog](https://github.com/sassanh/qnvim/compare/v10.0.0_1...v20.0.1)

**Changed**

- Bump Qt Creator 10.0.0 -> 20.0.1, Qt 6.2.4 -> 6.8.3, C++17 -> C++20.
- Update CI to Qt 6.8.3 / CMake 4.1.2 / Ninja 1.13.1 (windows-2022 / ubuntu-24.04 / macos-14, GCC 16), fix Homebrew Qt ABI mismatch.
- Fix `NVIM_LISTEN_ADDRESS` inheritance when launched from inside Neovim and guard `nvim_input` with `isReady` (API level 2).
- Add per-editor CmdLine widget and update LICENSE credits for Mikhail Zolotukhin (2022).
- Remove SPDX headers, LICENSES/, .reuse/ and reuse.yml; keep single LICENSE (2018-2026 Sassan Haradji, 2022 Mikhail Zolotukhin).

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
