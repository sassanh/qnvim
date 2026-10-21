# Changelog

All notable changes to this project will be documented in this file. This project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased](https://github.com/sassanh/qnvim/compare/v20.0.2...HEAD)

[Full Changelog](https://github.com/sassanh/qnvim/compare/v20.0.2...HEAD)

**Added**

- The command line, messages and completion menu can be rendered by Neovim plugins such as noice.nvim: **Preferences > QNVim > Command line, messages and completion menu** selects between **Rendered by QNVim** (the default) and **Rendered by Neovim plugins**. The setting is applied when qnvim starts; in plugin mode qnvim no longer claims `ext_messages`, `ext_cmdline`, `ext_popupmenu` or `ext_wildmenu`, so plugins that defer to the host UI can take over.

- `./quick-run.sh` runs the development loop in one command: it configures and builds the plugin, installs it into the Qt Creator downloaded under `external/qtcreator/` (retargeting it to the bundled Qt on macOS), stops a running instance and starts it with qnvim's logging enabled.

- Splits/windows support: Neovim's `:split`, `:vsplit` and any other window now show up as Qt Creator splits in the same arrangement as in Neovim (each one keeps its own file, cursor and viewport), and Qt Creator's own splits become Neovim windows. Closing a Neovim window removes its Qt Creator split; closing a split (or closing the last editor in it) closes its Neovim window ([#3](https://github.com/sassanh/qnvim/issues/3)). Known limitations: split sizes are not synced between the two, unnamed/scratch buffers get no editor, and Neovim tabpages are shown as splits in the same editor area.

- Floating windows (completion menus, hover popups, `fzf-lua`/`telescope` pickers, the dashboard, ...) are shown: each one appears as an overlay in Qt Creator's main window, with the text, colors and attributes Neovim reports for it. The main window is Neovim's screen, so a float Neovim anchors to the screen (the common picker or the dashboard, e.g.) is centered in it like in Neovim; a float anchored to a window — or one a plugin opened at the cursor's or the cmdline's cell of the editor's layout (`screenpos()`, nvim-cmp's menus) — is placed in the editor view that window is shown in, at the cursor or above the editor view's command line. The overlays are just views: they never take the keyboard focus and let the mouse through, so keys and clicks keep reaching Neovim.

**Fixed**

- Quitting Qt Creator no longer crashes in qnvim's shutdown when the editor that owned the line-number column was destroyed first: the column is a child of that editor, so it is now tracked with a `QPointer` and detached from an editor that is about to close, even if another editor has become current.

- Visual and visual-block selections in text with non-ASCII characters now cover the characters they are drawn on: the selection's cursor and anchor columns were sent to Neovim as UTF-16 code units instead of the UTF-8 byte columns it uses, so a line with accented or non-Latin characters before the selection put its edges too far left. Both are byte columns now, like the cursor synchronization already was. Visual-block selections that reach an empty line also keep that line as their edge instead of shifting it to the previous line.

- `:DebugContinue` continues the debugger: it triggered `ProjectExplorer.Continue`, which is not a Qt Creator action (the debugger's Continue action is `Debugger.Continue`), and triggering an unknown command id crashed Qt Creator. Unknown command ids now log a warning instead.

- By default, files opened from Neovim use Qt Creator's text editor for their type instead of a fixed suffix list, so files such as Python, CMake or JSON get Qt Creator's syntax highlighting. Files with no matching Qt Creator text editor still open as plain text.

- The relative line numbers stay aligned with their lines: each number is placed on the first visual row of its buffer line using the editor's own line geometry, so wrapped lines, folds and the partially visible top line no longer make the column drift. The column also uses the exact width of Qt Creator's line-number column, leaving the marks and folding columns untouched.

- Sync the editor viewport with Neovim's window: commands that scroll Neovim's window without moving the cursor (`zz`, `zt`, `zb`, `<C-e>`, `<C-y>`, `<C-d>`, `<C-u>`, `H`, `M`, `L`, ...) used to leave the Qt Creator editor view where it was, so they appeared to do nothing. The editor is now scrolled to Neovim's top line ([#15](https://github.com/sassanh/qnvim/issues/15)).

- `set wrap` works: Neovim now wraps lines where the Qt Creator editor wraps them, so `gj`/`gk` (and `j`/`k` with `'wrap'`) land on the visual line you see, instead of up to six columns off. Every window's text area is kept as wide as the editor view that shows it, and it follows along when windows are resized or when the line number and sign columns appear or disappear ([#4](https://github.com/sassanh/qnvim/issues/4)). When a wrap point falls on a space, the editor keeps that space at the end of the previous row while Neovim starts the next row with it, so the cursor can show up at the end of the previous drawn row there: it is kept on Neovim's exact position, on the character Neovim has it on. The editors also wrap by the mode of their document now, which the two halves of a Qt Creator split share: a new editor resets that mode to Qt's default (breaking at word boundaries), which used to leave the editors wrapping differently from Neovim until one of them became the current window.

- Help pages (`:help`) work and show their content: their editors used to crash Qt Creator (`std::bad_function_call`, the factory for them was missing its document creator) and the autocommand that was supposed to read the help files could never run, because qnvim's `BufReadCmd` autocommand suppresses Neovim's file reading. Help and terminal buffers now get proper plain text editors and the help files are read on Neovim's side.

- Closing one of two editors that show the same Neovim buffer (e.g. the two halves of a Qt Creator split) no longer deletes that buffer in Neovim: the remaining editor now takes the buffer over instead of losing it.

- `E516: No buffers were deleted` no longer appears when Neovim deletes a buffer that is open in Qt Creator (`:bd`, `:bw`, ...): qnvim used to ask Neovim to delete the buffer too while Neovim was already deleting it.

- Files whose names contain characters that are special in Ex commands (spaces, quotes, backslashes) can now be opened from Qt Creator: their names are escaped with Neovim's `fnameescape()` (previously Neovim replied `E32: No file name` and the editor ended up without a buffer).

- The Neovim command line in the editor view no longer keeps showing an old command or message: it is now cleared and hidden when Neovim clears its message.

- Cursor synchronizing from the editor side works with current Neovim: `SetCursor()` tested the mode with the `or` operator, which Neovim's expression parser rejects (`E488: Trailing characters: or ...`), so every cursor movement in a Qt Creator editor errored in Neovim.

- `tools/RetargetPluginQt.sh` derives the retargeted framework path from the original one instead of assuming a `Versions/A` layout, so it also handles frameworks with other layouts.

**Changed**

- Qt Creator's "Enable text wrapping" setting (Text Editor → Display) and Neovim's `wrap` option are now kept in sync: changing either one changes the other, so the editor's wrapping always agrees with Neovim. The setting follows the current window's option, while toggling the setting applies it to all Neovim windows (and to windows created later).

- The Neovim command line now lives in the editor view, like Neovim's own command line, instead of in Qt Creator's status bar: it is easier to notice, longer commands fit and Neovim messages are shown in full and can be scrolled. This finishes the per-editor command line widget by Mikhail Zolotukhin from [#57](https://github.com/sassanh/qnvim/pull/57), which was present in the code but never wired up ([#56](https://github.com/sassanh/qnvim/issues/56)).

## [20.0.2](https://github.com/sassanh/qnvim/tree/v20.0.2) (2026-10-18)

[Full Changelog](https://github.com/sassanh/qnvim/compare/v20.0.1...v20.0.2)

**Fixed**

- Fix a Qt Creator crash when Neovim is not running: opening a file or typing used to send requests over a connection that was not ready yet (dereferencing a null `NeovimApi2`). The connection's readiness is now checked before every request and connection errors are logged instead.
- Fix filetype detection never running for files opened in Qt Creator, which left `'filetype'` and filetype-plugin options such as `'commentstring'` empty for all filetypes ([#64](https://github.com/sassanh/qnvim/issues/64)).
- Fix the macOS compilation failure caused by the SDK's `usr/include` leaking into the include path through the `Cups::Cups`/`ZLIB::ZLIB` imported targets: a build directory reused after switching Xcode kept the previous SDK in the cached include paths, and that directory came before libc++'s headers, so `<cmath>` could not find libc++'s `<math.h>`.

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
