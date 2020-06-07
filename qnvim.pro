QT += network
DEFINES += QNVIM_LIBRARY

# QNVim files

SOURCES += qnvimplugin.cpp \
    numbers_column.cpp \

HEADERS += qnvimplugin.h \
    numbers_column.h \
    qnvim_global.h \
    qnvimconstants.h \

# Qt Creator linking

## Either set the IDE_SOURCE_TREE when running qmake,
## or set the QTC_SOURCE environment variable, to override the default setting
isEmpty(IDE_SOURCE_TREE): IDE_SOURCE_TREE = $$(QTC_SOURCE)

## Either set the IDE_BUILD_TREE when running qmake,
## or set the QTC_BUILD environment variable, to override the default setting
isEmpty(IDE_BUILD_TREE): IDE_BUILD_TREE = $$(QTC_BUILD)

## Either set the NEOVIM_QT_SOURCE_TREE when running qmake,
## or set the NEOVIM_QT_SOURCE_TREE environment variable, to override the default setting
isEmpty(NEOVIM_QT_SOURCE_TREE): NEOVIM_QT_SOURCE_TREE = $$(NEOVIM_QT_SOURCE_TREE)

## Either set the NEOVIM_QT_BUILD_TREE when running qmake,
## or set the NEOVIM_QT_BUILD_TREE environment variable, to override the default setting
isEmpty(NEOVIM_QT_BUILD_TREE): NEOVIM_QT_BUILD_TREE = $$(NEOVIM_QT_BUILD_TREE)

## uncomment to build plugin into user config directory
## <localappdata>/plugins/<ideversion>
##    where <localappdata> is e.g.
##    "%LOCALAPPDATA%\QtProject\qtcreator" on Windows Vista and later
##    "$XDG_DATA_HOME/data/QtProject/qtcreator" or "~/.local/share/data/QtProject/qtcreator" on Linux
##    "~/Library/Application Support/QtProject/Qt Creator" on macOS
##USE_USER_DESTDIR = yes

include($$IDE_SOURCE_TREE/src/qtcreatorplugin.pri)

INCLUDEPATH += $$NEOVIM_QT_SOURCE_TREE/src /usr/local/Cellar/msgpack/3.2.1/include
LIBS += -L$$NEOVIM_QT_BUILD_TREE/lib/ -L /usr/local/Cellar/msgpack/3.2.1/lib -lneovim-qt -lneovim-qt-gui -lmsgpackc
