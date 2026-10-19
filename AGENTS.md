# AGENTS.md

Notes for agents working on qnvim. They come from implementing split support
(#3) and debugging Neovim window ↔ Qt Creator editor view mapping.

## What this project is

qnvim embeds a real Neovim in Qt Creator. Qt Creator's own editor widgets
render the text; Neovim runs the editing engine (keys, plugins, buffers,
windows). The plugin connects Neovim windows to Qt Creator editor views
(splits), and Neovim buffers to Qt Creator documents.

Only Qt Creator's **public** API is used (`Core::EditorManager`, `Core::IEditor`,
...). Qt Creator sources are fine to read for understanding, but do not link
private headers/classes.

## Build, install, run (macOS dev loop)

The local Qt Creator to test with lives in `external/qtcreator/dist-Darwin-20.0.1/`
(not committed; version must match `external/qtcreator/version.cmake`). This
machine builds against Homebrew Qt, whose frameworks are linked by absolute
path, so **every rebuild** must be retargeted before the plugin can be loaded.

```sh
cmake --build build --target QNVim -j8

tools/RetargetPluginQt.sh   # rewrites Homebrew Qt paths to @rpath; re-signs

cp "build/Qt Creator.app/Contents/PlugIns/qtcreator/libQNVim.dylib" \
   "external/qtcreator/dist-Darwin-20.0.1/Qt Creator.app/Contents/PlugIns/qtcreator/libQNVim.dylib"
codesign --force --sign - \
   "external/qtcreator/dist-Darwin-20.0.1/Qt Creator.app/Contents/PlugIns/qtcreator/libQNVim.dylib"
```

Run with plugin logging enabled, from the repo root:

```sh
QT_LOGGING_RULES='qtcreator.plugin.qnvim.*=true' \
  "external/qtcreator/dist-Darwin-20.0.1/Qt Creator.app/Contents/MacOS/Qt Creator" \
  src/qnvimcore.h src/qnvimcore.cpp
```

- Plugin log lines come from `qDebug(Main)`; the category is
  `qtcreator.plugin.qnvim`.
- Crash reports: `~/Library/Logs/DiagnosticReports/`.
- Stopping the test instance: kill only
  `dist-Darwin-20.0.1/Qt Creator.app/Contents/MacOS/Qt Creator` and its child
  `nvim --embed --cmd let g:QNVIM=1`.
  **Never** kill other `nvim` processes — the user runs important servers
  (`~/tmp/neovim-sockets/qnvim`, `.../quiper`, `.../galley`).
- `external/qtcreator/dist-Darwin-20.0.1/Qt Creator.sdk/...` has the SDK
  headers used for reference (e.g. `coreplugin/editormanager/editormanager.h`).

## Working with the user on GUI behavior

- The user prefers to drive the GUI themselves. Do not send synthetic keys or
  clicks into their Qt Creator/Nvim session; ask them to perform one action and
  tell you the result. (This was an explicit request.)
- Ask for concrete, small checks: `:vsplit` once, `<C-w>w`, `:close`, a Qt-side
  split, etc. Then read the log.
- When diagnosing, add temporary `qDebug(Main) << "PREFIX ..."` lines, have
  the user reproduce **once**, read the prefix from the log, then remove the
  instrumentation before committing.

## Neovim integration facts

- The bundled NeovimQt bindings do **not** expose `nvim_exec`/`nvim_exec2`
  (`api2()`/`api6()`). Use `nvim_command`, `nvim_eval`,
  `nvim_call_function`, `nvim_open_win`, ...
- The whole VimL preamble is sent as one multi-line `nvim_command` string.
  Inside it, **a plain `autocmd ...` line absorbs the lines that follow it**
  (they become part of the autocmd's command; e.g. `QNVimWindowInfo` ends up
  undefined and calls fail with `E117`). Wrap every autocmd in
  `execute "autocmd ..."` (existing ones do this too). `function! ... endfunction`
  definitions are fine.
  Repro outside Qt Creator: send the same string to a fresh embedded nvim via
  `nvim_command` and check `exists('*QNVimWindowInfo')`. To distinguish a bad
  string from a bad delivery, run the string through `nvim_exec2(src,
  {output: true})` in the harness (the bindings do not expose `nvim_exec2`, but
  a harness can call it): if it works there, the multi-line `nvim_command`
  wrapping is the problem. A minimal harness spawns `nvim --embed` and speaks
  msgpack RPC over stdin/stdout (`nvim_command`, `nvim_eval`,
  `nvim_call_function`; notifications arrive as `Gui`/`redraw`).
- The socket exists only when Qt Creator is started with `QNVIM_LISTEN=<path>`:
  the plugin then passes `--listen <path>` to the spawned nvim. RPC queries are
  safe and are the way to inspect the live instance:
  `nvim --server <socket> --remote-expr '...'` (e.g. `screenstring()` /
  `screenchar()` read the plugin's own grid).
- Attaching a second UI (`nvim --server <socket> --remote-ui`, or `:connect`
  from another Nvim) generally works, but it has taken the backend down here:
  twice it crashed (heap corruption detected in `screen_resize` during the
  attach; Qt Creator then logs "The Neovim process has crashed" and nvim-move
  stops working until Qt Creator is restarted), while five other attaches of
  the same kind were fine. Use `:detach` (or close the terminal) to leave; keys
  like `:q`/`:qa`/`ZZ` act on the shared instance.
- Do not use `bufnr('')` after an asynchronous `:e` to learn what was opened:
  when several editors open at once, the current buffer may already belong to
  another file. Use `bufnr(<file name>)` (`nvim_call_function("bufnr", {filename})`).
- Window event semantics used by the plugin:
  - `WinNew`/`WinEnter`/`WinClosed` are wired to `QNVimWinEvent`; floats are
    filtered out with `win_gettype()`.
  - For `:split file`, `WinNew` reports the **old** buffer; `BufEnter` follows
    with the new one. A plain `:split` fires no `BufEnter`.
  - `WinClosed` fires before `WinEnter`; `win_getid()` is the dying window, so
    use `expand('<amatch>')` (already wired).
  - `nvim_open_win(..., {split: "right"|"below", win: ...})` fires
    `WinNew`/`WinEnter` and makes the new window current. The split direction
    is explicit and does not follow `'splitright'`/`'splitbelow'`.
- **Never assume a new window is right of / below its source.** Users may have
  `'splitright'`/`'splitbelow'` set, or use `:leftabove`/`:rightbelow`; Neovim
  can put the new window left of / above the old one. Read the actual layout.
- `getwininfo()` provides `winrow`, `wincol`, `width`, `height` (and floats,
  other tabpages). `width`/`height` of a bound window are the size qnvim
  requested for its grid (see below), not the space it takes in Neovim's
  layout, so window placement (`reconcileWindows()`) compares positions only:
  windows in the same row are side by side, windows in the same column are
  stacked, and the nearest bound candidate on either side is the source (both
  give the right result via the existing `before` swap). `QNVimWindowInfo()`
  returns position, size (same caveat), buffer, filename, tabpage and
  current-window flag.
- Every window's **grid is sized to the editor view that shows it**
  (`resizeWindow()`), so Neovim wraps lines where the editor wraps them. Neovim
  wraps at `grid width - win_col_off()`, so the grid is made
  `text columns + textoff` wide. Text columns are what
  `PlainTextDocumentLayout::layoutBlock()` lays the text out in: the viewport
  width minus the document margin on both sides and minus one more character
  (`TextEditorWidget::setDisplaySettings()` always enables
  `AddSpaceForLineAndParagraphSeparators`, which reserves the paragraph mark);
  the count uses 1/64th pixel arithmetic like Qt (`textColumns()`), otherwise
  Neovim wraps a couple of columns too late and `gj`/`j` land off the row they
  are on. The grid id is learned from `win_pos` (new windows only; floats
  emit `win_float_pos`) and `win_viewport` (pre-existing entries only — a
  float's id is not a window qnvim knows, which is what filters them);
  `textoff` comes from `getwininfo()` (it covers all tabpages, so hidden
  windows are known too). After the UI sizes a grid, Neovim keeps that size
  until the window is freed: re-apply it when the view resizes, when `textoff`
  changes (`:set number`, a sign appearing, ...) or the grid id changes.
  Qt's layout keeps a run of spaces that straddles a wrap point at the end of
  the previous row (the row then exceeds the width by the run), while Neovim
  always breaks at the cell, so such rows differ by the run length. Qt does this
  with any wrap mode (checked with a standalone `QTextLayout` program), so it
  cannot be avoided from here. A position Neovim considers the first cell of a
  row can therefore be drawn at the end of the previous row: the editor's cursor
  is put on Neovim's exact position all the same, so it can show up there.
- A window's grid **row 0 is its winbar** when one is set (dropbar sets a
  global one): Neovim's text area is the grid height minus the winbar, while
  `resizeWindow()` sizes the grid to the editor's rows, so with a winbar
  Neovim's text area is one row shorter than the editor view. Nothing handles
  that yet (nor the winbar itself: qnvim renders the document text, never grid
  content).
- `win_pos` is only reliable for *new* windows and at attach: a window whose
  layout changes later (e.g. the neighbour of `:vertical resize`) never gets
  one, so it cannot be used as a layout snapshot.

## Qt Creator editor area facts (20.0.1)

- `EditorManager::split()` splits vertically → new view **below**.
  `EditorManager::splitSideBySide()` splits horizontally → new view **right**.
- `SplitterOrView::split` keeps the original editor in the first (left/top)
  view and creates a **duplicate** editor for the new (right/bottom) view.
  The duplicate is made by `duplicateEditor` (it emits `editorCreated`, not
  `EditorManager::editorOpened`); activating the new view emits
  `currentEditorChanged`, which is what the plugin listens to.
- `EditorManager::activateEditorForDocument(doc)` looks for the document in the
  current view; if not found it activates `editors.first()` in that view. Qt
  Creator's `placeEditor` then either uses an editor already in that view,
  **duplicates only if the source editor is visible in its own view**, and
  otherwise **moves** the editor into the target view. Moving editors across
  views silently breaks window ↔ view mappings.
- Consequence: when creating a view for a Neovim window, keep the editor the
  split itself created in the new view. Do not "open the buffer" to get an
  editor for the new view when the duplicate already shows the buffer.
- The "Enable text wrapping" display setting is kept in sync with Neovim's
  `'wrap'`: the setting follows the current window's option (written quietly,
  `Utils::BaseAspect::BeQuiet`, see `syncFromVim()`), and toggling the setting
  in the UI applies it to every window (`tabdo windo setlocal`, plus
  `setglobal` for new ones, see `wrapSettingChanged()`). Qt Creator applies
  display setting *changes* to **all** open editors (each `TextEditorWidget`
  connects to `DisplaySettings::changed`), which is why qnvim writes the
  setting without announcing: other windows' editors must keep following
  their own Neovim windows.
- Qt wraps by the **text document**, not by the widget:
  `QPlainTextEdit::wordWrapMode()` is per widget, but the layout uses
  `QTextDocument::defaultTextOption().wrapMode()`, and the editors of one
  document (the two halves of a split) **share** that document. A new editor
  attaching to a document resets the document's mode to Qt's default
  (`WrapAtWordBoundaryOrAnywhere`) — `QPlainTextEdit::setDocument()` calls
  `updateDefaultTextOption()`. The widget's own value keeps reporting what
  qnvim set, so the document has to be set as well (`applyWrapMode()` in
  `src/qnvimcore.cpp`); otherwise the editors wrap at word boundaries while
  Neovim wraps at the cell.
- There is no public "duplicate this document into this view" API; a Qt split's
  duplicate is the only convenient way to get a second editor for a document.
- `EditorManager::openEditor` returns an existing editor for the file and may
  switch splits/activate it; flags include `OpenInOtherSplit`,
  `SwitchSplitIfAlreadyVisible`, `DoNotChangeCurrentEditor`.
- The plugin identifies a view as `editor->widget()->parentWidget()->parentWidget()`
  (editor stack → `EditorView`) and the current editor of a view via
  `EditorManager::visibleEditors()`. Views are the stable unit across splits;
  the `EditorView` widget of the original split stays the same.

## Plugin architecture (src/qnvimcore.*)

- Model: each Neovim window ↔ one Qt Creator editor view; a document ↔ one
  Neovim buffer, shared by all editors showing that document (a Qt split has
  its own editor for the same document).
- State:
  - `mWindows`: win id → `WindowState` (bound, editor, view, buffer, cursor,
    viewport/topline).
  - `mNewWindows`: windows seen `WinNew` but not yet bound, reconciled later.
  - `mBuffers` (editor → buffer) and `mEditors` (buffer → editor).
  - `mCurrentWindow`: Neovim's current window.
- Neovim → Qt: `winAutoCommand` notifications; `reconcileWindows()` polls
  `QNVimWindowInfo()` on redraw flush when `mWindowsDirty`, creates views for
  new windows (`createViewForWindow`) and binds windows.
- Qt → Neovim: `editorOpened` (connected to `currentEditorChanged`),
  `editorAboutToClose`, `ensureWindowForView()` creates a Neovim window for a
  new Qt view.
- Mapping invariants:
  - For a bound window, `st.editor` shows `st.buffer` and lives in `st.view`.
  - A view has at most one window; use `windowForView`/`windowForEditor`.
  - When Neovim places a new window **before** (left/above) its source but
    Qt Creator splits after, the two views are the right pair: swap the
    bindings (`bindWindow(newWin, sourceEditor)` and
    `bindWindow(sourceWin, duplicateEditor)`), then `activateWindowEditor(newWin)`.
    This keeps pane order identical to Neovim's layout.
- Reentrancy guards: `mInhibitWindowSync`, `mCreatingViewForWindow`,
  `mCreatingWindowForView`, `mRemovingView`, `mVimInitiatedClose`. Save and
  restore them around calls that can run a nested event loop
  (`Core::EditorManager` operations, `closeEditors`, prompts).
- Async pitfalls: lambdas may outlive their editor — guard with
  `QPointer<Core::IEditor>` for delayed work (`QTimer::singleShot`, ...).
  `fixSize()` must ignore null/non-text editors; resizing after a split is
  torn down otherwise crashes Qt Creator.
- Closing: `WinClosed` removes the Qt split via
  `QtCreator.RemoveCurrentSplit`; editing side, closing the last editor of a
  view closes its window (never the last window). Editors are closed without
  deleting shared buffers.

## Known limitations (keep docs in sync)

- Split sizes are not synced (moving the Qt splitter does not resize the
  Neovim window, and vice versa has edge cases).
- Neovim tabpages are all shown as splits in the same editor area.
- Unnamed/scratch buffers get no editor; windows without a filename are
  reconciled when they get one.
- A window can only be represented once Qt Creator has an editor for its
  buffer; windows Neovim creates for existing documents reuse editors.

Update `CHANGELOG.md` (Unreleased → Added/Fixed) and `README.md` (Status) when
behavior or limitations change.

## Repository conventions

- Commit messages: `feat:`/`fix:`/`docs:` + a one-line summary, then bullet
  points; reference `(#<issue>)` where relevant. Do not push unless asked.
- Local commits on `master` are fine for this project.
- Keep the SDK-only/public-API approach; do not add private Qt Creator headers.
- Remove temporary diagnostics (prefixed qDebug output, file dumps, one-shot
  verification evals) before committing.
