<!--SPDX-FileCopyrightText: none-->
<!--SPDX-License-Identifier: CC0-1.0-->

# qnvim

qnvim is a Qt Creator plugin for users who like editing text in Neovim/Vim and also want to use Qt Creator features. This plugin combines the power of Neovim and Qt Creator.

With qnvim, you can run your `init.lua`/`init.vim`, all your Neovim plugins, and any tweaks made in `init.vim`/`init.lua`.

<p align="center">
  <a href="https://www.youtube.com/watch?v=twwnnduujzw">
    <img src="https://user-images.githubusercontent.com/1270688/51085365-02e51900-174d-11e9-92f7-c6daa5ec33de.gif"/>
  </a>
</p>

## Status

qnvim is under development, but it's mostly stable and usable. Currently, there are some known issues:

- It doesn't support splits or windows
- It should use vim highlights for buffers that Qt Creator doesn't support (like Vim helpfiles and many others)

Most Neovim plugins should work fine, except for a few that rely on highlights or special buffers. Qt Creator already provides excellent highlighting for C++ and QML, so that's not a problem. Work is in progress to handle all types of buffers.

Please report any issues you encounter and consider contributing to this project if you have the time.

## How does qnvim compare with Qt Creator Vim mode?

qnvim provides a smoother integration of Neovim within Qt Creator by running an actual instance of Neovim. This allows you to use all your Neovim plugins and customizations directly in Qt Creator. On the other hand, Qt Creator Vim mode is a built-in feature that emulates Vim, offering basic Vim keybindings and functionality but does not support the full range of Neovim features and plugins. The main difference between the two is that qnvim runs a real instance of Neovim, while Qt Creator Vim mode is an emulation of Vim.

## Installation instructions

### From Releases section

Go to the releases section and download the version of the plugin matching your Qt Creator version and operating system. Then:

1. Open Qt Creator > Help > About Plugins > Install Plugin...
2. Select the plugin you've downloaded earlier and relaunch Qt Creator.

### Building from source

> ⚠️ **Warning** ⚠️
>
> As per Qt policies, major and minor versions of Qt Creator Plugin APIs are not compatible. This means that there is no guarantee that the plugin version on the master branch is compatible with any version of Qt Creator not specified in the cmake/FetchQtCreator.cmake file.

1. Make sure you have Qt development files installed on your system.
2. Clone this repository and go to its directory. Checkout a Git tag that is compatible with your Qt Creator version.
3. `cmake -S . -B build/`.
4. `cmake --build build/`. The compiled plugin will be inside `build/lib/qtcreator/plugins`.
5. Open Qt Creator > Help > About Plugins > Install Plugin... Select the plugin you have built earlier.

#### Updating

Before updating from source, delete the `build` directory from earlier to avoid problems such as [this](https://github.com/sassanh/qnvim/issues/8#issuecomment-485456543).

To update the plugin you need to recompile it: checkout a tag, that matches your Qt Creator version and execute the steps above again.

### Arch Linux

Arch Linux users can install [qnvim-git](https://aur.archlinux.org/packages/qnvim-git) from AUR via AUR helper or with the following commands:

```bash
git clone https://aur.archlinux.org/qnvim-git.git
cd qnvim-git
makepkg -si
```

## Configuration

You can add custom Vim commands for your Qt Creator environment in a `qnvim.vim` file located in the same directory as `init.vim` (`:help $MYVIMRC`). `$MYQVIMRC` is set to the path (mind the `Q` after `MY`).

### Sample `qnvim.vim`

There's a sample `examples/qnvim.vim` file available in the repository. It provides most of the convenient keyboard shortcuts for building, deploying, running, switching buffers, switching tabs, and more. It will also help you understand how to create new keyboard shortcuts using Qt Creator commands.

## Credits

- [Neovim](https://neovim.io)
- [Qt](https://www.qt.io)
- [Qt Creator](https://www.qt.io/product)
- [Neovim Qt](https://github.com/equalsraf/neovim-qt)

And the libraries used in above projects and are mentioned in their docs.
