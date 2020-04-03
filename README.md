# qnvim

A Qt Creator plugin that helps those who are used to editing text in vim and also need Qt Creator features to combine these two.

It runs an instance of Neovim inside Qt Creator, so it's able to run your `init.vim` and all your Neovim plugins and your tweaks in `init.vim` should work.

<p align="center">
  <a href="https://www.youtube.com/watch?v=twwnnduujzw">
    <img src="https://user-images.githubusercontent.com/1270688/51085365-02e51900-174d-11e9-92f7-c6daa5ec33de.gif"/>
  </a>
</p>

## Status

It's in beta but it's mostly stable and usable, for last few months I've been using Qt Creator only with this plugin with no pain. Currently these are the known issues:

- It doesn't support splits nor windows
- It should use vim highlights for buffers that Qt Creator doesn't support. (like Vim helpfiles and many others.)

All my 149 plugins installed in Neovim work alright except a few that relate on highlights (Qt Creator highlights C++ and QML better than any Vim plugin, so it's totally alright.) and those that rely on special buffers. (Work is in progress to handle all types of buffers.)

Please let me know if you find any problems and please contribute to this project if you have the time.

## Installation instructions

### Building from source

1. Build Qt Creator from git repository, see instructions [here](https://wiki.qt.io/Building_Qt_Creator_from_Git). If you build the head of master branch, then you can use qnvim only with your build, if you wanna use qnvim with the official release of Qt Creator you should checkout the tag that matches your installed Qt Creator.
2. Clone and compile [Neovim Qt](https://github.com/equalsraf/neovim-qt).
3. Install devel package of msgpack.
4. Setup locations to Qt Creator and Neovim Qt. You can manually edit it in `qnvim.pro` or pass `IDE_SOURCE_TREE`, `IDE_BUILD_TREE`, `NEOVIM_QT_SOURCE_TREE` and `NEOVIM_QT_BUILD_TREE` with corresponding paths to `qmake`. If you want to install this plugin user-wide you need to pass `USE_USER_DESTDIR` or uncomment in in `qnvim.pro`
5. Build. You can just open `qnvim.pro` in Qt Creator and build the plugin from it. Or you can do it from CLI by executing the following in qnvim source tree:

```bash
mkdir build
cd build
qmake .. # Here you can pass parameters, for example NEOVIM_QT_SOURCE_TREE=../neovim-qt
make
```

6. Put the built library in the location that Qt Creator expects plugins (it varies based on your OS) and use the built Qt Creator instead of the official version. (Or if you checked out the tag that corresponds to your installed Qt Creator, then you can use your own installed Qt Creator.)

**Updating**

If you update your Qt Creator, you need to build qnvim against the version of Qt Creator you updated to. For example if you update from 4.8.0 to 4.9.0, you should go to the directory you clonned Qt Creator in, run `git fetch` followed by `git checkout <VERSION> --recurse`. Then you should go to the build directory of Qt Creator and **run `make clean`** and then build Qt Creator again. Then you should clean and build qnvim.

If you want to update qnvim, you just need to `git pull` in its directory and build it again, no need to do anything with Qt Creator build.

**Always run `make clean` before building anything after you upgrade Qt Creator**

It's important to run `make clean` before building Qt Creator when it's upgraded, otherwise you'll end up with plugins with mismatching versions like [this](https://github.com/sassanh/qnvim/issues/8#issuecomment-485456543).

### Arch Linux

Arch Linux users can install [qnvim-git](https://aur.archlinux.org/packages/qnvim-git) from AUR via AUR helper or with the following commands:

```bash
git clone https://aur.archlinux.org/qnvim-git.git
cd qnvim-git
makepkg -si
```

## Configuration

You can put custom Vim commands for your Qt Creator environment in `qnvim.vim` which is a file in the same directory as `init.vim` (`:help $MYVIMRC`). `$MYQVIMRC` (note the `Q` after `MY`) is set to its path.

### Sample `qnvim.vim`

There's a sample `qnvim.vim` file available in the repo, it provides most of the convenient keyboard shortcuts for building, deplying, running, switching buffers, switching tabs, etc. It'll also help you understand how you can create new keyboard shortcuts using Qt Creator commands.

## Credits

- [Neovim](https://neovim.io): The lovely text editor that solves the problem of writing AND editing text once for ever.
- [Qt](https://www.qt.io): The lovely cross platform framework that lets you build awesome software with its full stack of tools from networking to ui controls to 2d/3d graphics and many others.
- [Qt Creator](https://www.qt.io/product): The lovely IDE that lets you use the full power of Qt in an user friendly environemnt (I rememeber days that Qt Creator didn't exist and to code with Qt you had to use Eclipse or bare terminal and it was like a nightmare, Qt Creator eased the programming with Qt a lot.)
- [Neovim Qt](https://github.com/equalsraf/neovim-qt)

And lots of other libraries that are used in above projects and are mentioned in their docs.
