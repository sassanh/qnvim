# qnvim
Neovim backend for Qt Creator

# Install Instructions
1. Build Qt Creator from git repository (https://wiki.qt.io/Building_Qt_Creator_from_Git)

If you build the head of master branch, then you can use qnvim only with your build, if you wanna use qnvim with the official release of Qt Creator you should checkout the tag that matches your installed Qt Creator.

2. Clone and compile https://github.com/equalsraf/neovim-qt
3. Install devel package of msgpack.
4. Open the `qnvim.pro` file as a project in your offical Qt Creator (not the one you built.)
5. Setup locations to Qt Creator build and neovim-qt in `qnvim.pro` file.
Like this:
```
isEmpty(IDE_SOURCE_TREE): IDE_SOURCE_TREE = "<PATH_TO_QT_CREATOR_CLONED_REPO>"
isEmpty(IDE_BUILD_TREE): IDE_BUILD_TREE = "<PATH_TO_QT_CREATOR_BUILD_DIRECTORY>"
INCLUDEPATH += <PATH_TO_NEOVIM_QT_SRC_DIRECTORY> <PATH_TO_MSGPACK_HEADERS>
LIBS += -L<PATH_TO_NEOVIM_QT_LIB_DIRECTORY> -lneovim-qt -lneovim-qt-gui -L<PATH_TO_MSG_PACK_LIB_DIRECTORY> -lmsgpackc
```
For example:
```
isEmpty(IDE_SOURCE_TREE): IDE_SOURCE_TREE = "/home_direcotry/packages/qt-creator"
isEmpty(IDE_BUILD_TREE): IDE_BUILD_TREE = "/home_direcotry/packages/qt-creator-build"
INCLUDEPATH += /home_direcotry/packages/neovim-qt/src /usr/local/Cellar/msgpack/2.1.5/include
LIBS += -L/home_direcotry/packages/neovim-qt/build/lib -lneovim-qt -lneovim-qt-gui -L/usr/local/Cellar/msgpack/2.1.5/lib -lmsgpackc
```

6. Setup locations to your Qt Creator build in Projects tab.
In the Qt Creator go to `Projects` Tab/Mode (you can select it in the right column) Active project should be `qnvim`, and in Build & Run `Desktop ...` should be selected. Under `Desktop ...` select `Run` and in the right side in the `Run` section set Executable to `<full path to>/qt-creator-build/bin/qtcreator` and working directory to `<full path to>/qt-creator-build/bin`

7. Build and run the project.

8. (Optional) Put the built library in the location that Qt Creator expects plugins (it varies based on your OS) and use the built Qt Creator instead of the official version. (Or if you checked out the tag that corresponds to your installed Qt Creator, then you can use your own installed Qt Creator.)

## Credits
### Qt
The lovely cross platform framework that lets you build awesome software with its full stack of tools from networking to ui controls to 2d/3d graphics and many others.

### Neovim
The lovely text editor that solves the problems of editing text once for ever.

### Qt Creator
The lovely IDE that lets you use the full power of Qt in an user friendly environemnt (I rememeber days that Qt Creator didn't exist and to code with Qt you had to use Eclipse or bare terminal and it was like a nightmare, Qt Creator eased the programming with Qt a lot.)

### Qt Neovim
The C++ binding for neovim's msgpack communication layer.

### And lots of other libraries that are used in above projects
