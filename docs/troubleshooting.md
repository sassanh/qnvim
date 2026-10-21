# Troubleshooting

## The plugin does not load

Check that the plugin's Qt Creator version matches your Qt Creator. Qt Creator's
plugin API is not compatible across major/minor versions. The version targeted
by `master` is in
[`external/qtcreator/version.cmake`](../external/qtcreator/version.cmake), and
release archives include the Qt Creator version in their names.

Also check that *QNVim* is enabled in *Help > About Plugins* (*Qt Creator >
About Plugins* on macOS).

See [Building and installing](building.md) for installation issues.

## Neovim does not start

Run `nvim --version` in a terminal. qnvim spawns `nvim` from `PATH`; make sure
the same `PATH` is visible to Qt Creator when it is launched from the desktop.

## macOS: `Cannot load library ... Symbol not found ...`

The plugin was built against a Qt that links its frameworks by absolute path.
Build against an official Qt, or run
[`tools/RetargetPluginQt.sh`](../tools/RetargetPluginQt.sh) on the built plugin
(and again after every rebuild).

## Logs

Start Qt Creator with qnvim's logging enabled:

```sh
QT_LOGGING_RULES='qtcreator.plugin.qnvim.*=true' qtcreator
```

On macOS, launch the executable directly:

```sh
QT_LOGGING_RULES='qtcreator.plugin.qnvim.*=true' \
  "Qt Creator.app/Contents/MacOS/Qt Creator"
```

On Windows, set `QT_LOGGING_RULES` in the environment before starting Qt
Creator.

## Inspecting the embedded Neovim

Start Qt Creator with `QNVIM_LISTEN=/path/to/socket`; qnvim then passes
`--listen /path/to/socket` to the Neovim it spawns. Query it from another
process:

```sh
nvim --server /path/to/socket --remote-expr 'line(".")'
```

Do not attach a second UI with `--remote-ui`; that has crashed the backend
during development.
