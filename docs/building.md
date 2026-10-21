# Building and installing

## Requirements

- **Qt Creator** matching the plugin version. Qt Creator's plugin API is only
  compatible within a major/minor release, so a plugin built for one version is
  not guaranteed to load in another. The version targeted by `master` is in
  [`external/qtcreator/version.cmake`](../external/qtcreator/version.cmake)
  (currently Qt Creator 20.0.1).
- **Neovim** in `PATH`. qnvim is developed against Neovim 0.12.6; older releases
  may work but are not tested.
- To build: Qt 6.8+ development files, CMake 3.22+, a C++20 compiler, and
  network access on the first configure.

Windows, Linux and macOS are supported.

## From a release

Download the archive from the
[releases page](https://github.com/sassanh/qnvim/releases) that matches your
Qt Creator version and operating system. Release archives are built from tags
and may lag behind `master`.

1. Open *Help > About Plugins* (*Qt Creator > About Plugins* on macOS) and
   choose **Install Plugin...**.
2. Select the downloaded archive and follow the wizard. Relaunch Qt Creator
   when it asks.

## From source

1. Install Qt 6.8+ development files, CMake 3.22+ and a C++20 compiler.
2. Clone the repository and check out a tag that matches your Qt Creator
   version. (`master` tracks the version in
   `external/qtcreator/version.cmake`, currently Qt Creator 20.0.1.)
3. Configure and build:

   ```sh
   cmake -S . -B build
   cmake --build build
   ```

   On the first configure, CMake downloads the matching Qt Creator development
   files into `external/qtcreator/dist-<platform>-<version>/`. If you already
   have a Qt Creator package with development files, point CMake at it instead:

   ```sh
   cmake -S . -B build -DFETCH_QTC=OFF -DCMAKE_PREFIX_PATH=/path/to/qtcreator
   ```

4. The plugin is built at:

   - macOS: `build/Qt Creator.app/Contents/PlugIns/qtcreator/libQNVim.dylib`
   - Linux and Windows: under `build/lib/qtcreator/plugins/`
     (`libQNVim.so` or `QNVim.dll`; multi-config generators add a
     `Debug`/`Release` subdirectory)

5. Install it through *Help > About Plugins > Install Plugin...* and relaunch
   Qt Creator.

### macOS

When the plugin loads, it resolves Qt from the Qt Creator it is installed into,
so it can be built with a Qt newer than the version above as long as it is an
official Qt build (Qt installer or
[aqt](https://github.com/miurahr/aqtinstall)). Qt builds that link their
libraries by absolute path (for example Homebrew's) produce plugins that fail to
load with a symbol error. For those,
[`tools/RetargetPluginQt.sh`](../tools/RetargetPluginQt.sh) can fix an
already-built plugin; see the comments at the top of the script.

## Development build

[`quick-run.sh`](../quick-run.sh) runs the whole loop on macOS and Linux: it
configures and builds the plugin, installs it into the Qt Creator downloaded
under `external/qtcreator/` (retargeting it to the Qt bundled in Qt Creator on
macOS), stops the test instance and starts it with qnvim's logging enabled.

```sh
./quick-run.sh                     # build and run
./quick-run.sh src/qnvimcore.cpp   # build, run and open a file
```

The first run downloads the Qt Creator development package. Only the test Qt
Creator under `external/qtcreator/` is touched; a Qt Creator installed elsewhere
(for example in `/Applications`) is left alone.

## Updating

Check out the tag matching your new Qt Creator version and build again. If a
build misbehaves after switching Qt Creator or Qt versions, delete the `build`
directory and configure from scratch to discard stale cached paths.

## Arch Linux

Arch Linux users can install
[qnvim-git](https://aur.archlinux.org/packages/qnvim-git) from the AUR with an
AUR helper or:

```sh
git clone https://aur.archlinux.org/qnvim-git.git
cd qnvim-git
makepkg -si
```
