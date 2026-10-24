---
name: qnvim-gui-testing
description: Test GUI mode — protocol test against real Neovim, offscreen smoke test of the Shell widget with a screenshot, and the manual checklist in Qt Creator. Use after changes to qnvim_gui.lua, the session, the startup arguments, or before a release.
---

# Testing GUI mode

## 1. Protocol test (fast, no Qt, no Qt Creator)

```sh
pip install pynvim
python3 tests/gui/test_protocol.py --nvim "$(command -v nvim)"
```

Starts Neovim like the plugin (`--embed`, Lua glue via `--cmd`) and plays
Qt Creator's role over msgpack-rpc. Covers: startup `bufenter`, `add` without
switching, `open` with line/column (UTF-16 → byte), avoiding special windows,
jumping to an existing window, `modified`/`written`/writing to disk, silent
`close`, `bufdelete` on `:bd`, debounced `cursor`, floats without `bufenter`,
`:Qtc` + shortcuts + completion.

New test: add a method to `GuiProtocolTest`; trigger an action, then
`ev = self.events()` (reads up to a sentinel) and check the tuple.

## 2. Smoke test (real Shell widget, offscreen)

```sh
cmake -S . -B build-check -DQNVIM_BUILD_TESTS=ON -DQNVIM_TEST_NVIM="$(command -v nvim)" ...
cmake --build build-check --target qnvim_gui_smoke_run
# or: tools/compilecheck/run.sh --tests
```

Checks: UI attaches, shim loaded (`:GuiFont`), Lua glue loaded, `bufenter`
arrives, grid has content and the right size, completion menu is drawn by
Neovim. Screenshots: `build-check/tests/gui/smoke.png` and `smoke-pum.png` —
**look at them** (Read tool), don't just read PASS: syntax colors, statusline,
cmdline at the bottom, popup under the cursor.

## 3. Manual checklist in Qt Creator (before release)

Start with `QT_LOGGING_RULES='qtcreator.plugin.qnvim.*=true'`.

- [ ] Open project, open a file from the project tree → Neovim shows it, focus is in Neovim
- [ ] Locator (`:Locator`) → file:line jumps to the line
- [ ] Click a compile error → correct line/column
- [ ] Modify → `*` in the tab/dropdown; `Ctrl+S` in Neovim (`:w`) → `*` gone
- [ ] Modify, `:Build` → Qt Creator saves first (file on disk is up to date)
- [ ] `:e other.cpp`, Telescope/fzf-lua → Qt Creator's current document follows
- [ ] `:bd` → document disappears from "Open Documents"
- [ ] Close the document in Qt Creator (modified, "Do not Save") → buffer gone
- [ ] Completion (nvim-cmp/blink), LSP hover, which-key, noice → visible as in nvim
- [ ] `:Qtc` without argument → picker; `:Qtc Debugger.<Tab>` completes
- [ ] `Ctrl+2`/`Alt+4` → Qt Creator mode/output (pass-through)
- [ ] `.ui` file still opens in Designer
- [ ] "Open With" → Qt Creator text editor still works
- [ ] `:qa` → documents close, the next file starts a fresh Neovim
- [ ] Broken init.lua (`error()` at the top) → message, no restart loop
- [ ] Quit Qt Creator with a modified file → save dialog, saving writes
- [ ] Switch mode to "Classic" + restart → old text editors, no more Neovim mappings

## Reporting the result

Which stages ran, PASS/FAIL per check, the screenshot paths, and what could
**not** be tested (e.g. no Qt Creator available → stage 3 pending).
