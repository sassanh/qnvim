<!--SPDX-FileCopyrightText: none-->
<!--SPDX-License-Identifier: CC0-1.0-->

# qnvim

A Qt Creator plugin that helps those who are used to editing text in Neovim/Vim
and also need Qt Creator features to combine these two.

It runs an instance of Neovim inside Qt Creator, so it's able to run your
`init.lua`/`init.vim` and all your Neovim plugins and your tweaks in
`init.vim`/`init.lua` should work.

<p align="center">
  <a href="https://www.youtube.com/watch?v=twwnnduujzw">
    <img src="https://user-images.githubusercontent.com/1270688/51085365-02e51900-174d-11e9-92f7-c6daa5ec33de.gif"/>
  </a>
</p>

## Status

qnvim is in development, but it's mostly stable and usable. Currently these are
the known issues:

- It doesn't support splits nor windows
- It should use vim highlights for buffers that Qt Creator doesn't support.
  (like Vim helpfiles and many others.)

Neovim plugins should work alright except a few that relate on highlights (Qt
Creator highlights C++ and QML better than any Vim plugin, so it's totally
alright.) and those that rely on special buffers. (Work is in progress to
handle all types of buffers.)

Please report any problems you encounter and consider contributing to this
project if you have the time.

## Installation instructions

### From Releases section

Go to the [releases section](https://github.com/sassanh/qnvim/releases)
and download the version of the plugin matching your Qt Creator version
and operating system. Then:

1. Open Qt Creator > Help > About Plugins > Install Plugin...
2. Select the plugin you've downloaded earlier and relaunch Qt Creator.

### Building from source

> ⚠️ **Warning** ⚠️ 
>
> As per [Qt policies](https://doc.qt.io/qtcreator-extending/coding-style.html#binary-and-source-compatibility)
> major and minor versions of Qt Creator Plugin APIs are not compatible,
> meaning that there is no guarantee that the plugin version on the master
> branch is compatible with any version of Qt Creator not specified in
> `cmake/FetchQtCreator.cmake` file.

1. Ensure you have Qt development files installed in your system.
2. Clone this repository and go to its directory. Checkout a Git tag, that is
   compatible with your Qt Creator version.
3. `cmake -S . -B build/`.
4. `cmake --build build/`. The compiled plugin will be inside
   `build/lib/qtcreator/plugins`.
5. Open Qt Creator > Help > About Plugins > Install Plugin... Select the plugin
   you have built earlier.

#### Updating

Before updating from source, delete the `build` directory from earlier to
avoid problems such as
[this](https://github.com/sassanh/qnvim/issues/8#issuecomment-485456543).

To update the plugin you need to recompile it: checkout a tag, that matches
your Qt Creator version and execute the steps above again.

### Arch Linux

Arch Linux users can install
[qnvim-git](https://aur.archlinux.org/packages/qnvim-git) from AUR via AUR
helper or with the following commands:

```bash
git clone https://aur.archlinux.org/qnvim-git.git
cd qnvim-git
makepkg -si
```

## Configuration

You can put custom Vim commands for your Qt Creator environment in `qnvim.vim`
which is a file in the same directory as `init.vim` (`:help $MYVIMRC`).
`$MYQVIMRC` (note the `Q` after `MY`) is set to its path.

### Sample `qnvim.vim`

There's a sample `examples/qnvim.vim` file available in the repo, it provides
most of the convenient keyboard shortcuts for building, deploying, running,
switching buffers, switching tabs, etc. It'll also help you understand how you
can create new keyboard shortcuts using Qt Creator commands.

## Credits

- [Neovim](https://neovim.io)
- [Qt](https://www.qt.io)
- [Qt Creator](https://www.qt.io/product)
- [Neovim Qt](https://github.com/equalsraf/neovim-qt)

And the libraries used in above projects and are mentioned in their docs.
