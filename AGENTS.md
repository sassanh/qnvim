# AGENTS.md

qnvim is a Qt Creator plugin that embeds a real Neovim. Neovim runs the
editing engine; Qt Creator's editor widgets render the text and provide the
views (splits). Neovim windows map to editor views, buffers to documents.

Use only Qt Creator's public API (`Core::EditorManager`, `Core::IEditor`, ...).
Qt Creator sources are fine to read; do not link private headers.

## Build and run (macOS)

The test Qt Creator lives in `external/qtcreator/dist-Darwin-20.0.1/` (not
committed; its version must match `external/qtcreator/version.cmake`). The
build here uses Homebrew Qt, whose frameworks are linked by absolute path, so
every rebuild must be retargeted before the plugin can load.

`./quick-run.sh` does the whole loop below (configure, build, retarget, install,
restart the test Qt Creator with `qtcreator.plugin.qnvim.*=true`). The manual
commands remain here for when it fails or a variation is needed:

```sh
cmake --build build --target QNVim -j8
tools/RetargetPluginQt.sh
cp "build/Qt Creator.app/Contents/PlugIns/qtcreator/libQNVim.dylib" \
   "external/qtcreator/dist-Darwin-20.0.1/Qt Creator.app/Contents/PlugIns/qtcreator/libQNVim.dylib"
codesign --force --sign - \
   "external/qtcreator/dist-Darwin-20.0.1/Qt Creator.app/Contents/PlugIns/qtcreator/libQNVim.dylib"
```

Run from the repo root with plugin logging:

```sh
QT_LOGGING_RULES='qtcreator.plugin.qnvim.*=true' \
  "external/qtcreator/dist-Darwin-20.0.1/Qt Creator.app/Contents/MacOS/Qt Creator" \
  src/qnvimcore.h src/qnvimcore.cpp
```

- Logs use `qDebug(Main)`; the category is `qtcreator.plugin.qnvim`.
- SDK headers: `external/qtcreator/dist-Darwin-20.0.1/Qt Creator.sdk/...`.
- Crash reports: `~/Library/Logs/DiagnosticReports/`.
- Stop only the test Qt Creator and its child
  `nvim --embed --cmd let g:QNVIM=1`. **Never kill other `nvim` processes** —
  the user runs important servers (e.g. under `~/tmp/neovim-sockets/`).
- Start Qt Creator with `QNVIM_LISTEN=<path>` to make the spawned Neovim
  listen on a socket; inspect it with `nvim --server <path> --remote-expr '...'`.
  Do not attach a second UI: `--remote-ui` has crashed the backend here.

## Working on GUI behavior

- The user drives the GUI. Do not send synthetic keys or clicks into their
  Qt Creator/Neovim session; ask for one small action (`:vsplit`, `<C-w>w`,
  `:close`, a Qt-side split, ...) and read the log.
- To diagnose, add temporary `qDebug(Main) << "PREFIX ..."` lines, have the
  user reproduce once, read the prefix, then remove the instrumentation before
  committing.

## Gotchas

- The bundled NeovimQt bindings do not expose `nvim_exec`/`nvim_exec2`. Use
  `nvim_command`, `nvim_eval`, `nvim_call_function`, `nvim_open_win`, ...
- The VimL preamble is sent as one multi-line `nvim_command`. A plain
  `autocmd ...` line absorbs the lines that follow it, so wrap every autocmd
  in `execute "autocmd ..."`; `function! ... endfunction` is fine.
- The Lua API (`src/qnvim.lua`) reaches Neovim as a `--cmd` that starts with
  `lua package.preload...`. `:lua` takes the rest of the command string,
  newlines included, as its chunk, so the module must stay the last thing in
  that command.
- Never assume a new Neovim window is right of/below its source:
  `'splitright'`/`'splitbelow'` and `:leftabove`/`:rightbelow` can put it on
  either side. Read the actual layout.
- Do not use `bufnr('')` after an asynchronous `:e`; the current buffer may
  already be another file. Use `bufnr(<file name>)`.
- A window's grid is sized to the editor view that shows it so wrapping
  matches. Grid ids, `textoff` and resizes are coupled; read
  `resizeWindow()`/`textColumns()` in `src/qnvimcore.cpp` before changing them.

## Repository conventions

- Commit messages: `feat:`/`fix:`/`docs:` + one-line summary, then bullet
  points; reference `(#<issue>)` where relevant. Do not push unless asked.
  Local commits on `master` are fine.
- Keep the public-API-only approach.
- Remove temporary diagnostics before committing.
- Update `CHANGELOG.md` (Unreleased → Added/Fixed) and the relevant
  documentation (`docs/features.md`, `docs/limitations.md`, ...) when behavior
  or limitations change.
