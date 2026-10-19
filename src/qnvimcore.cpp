#include "qnvimcore.h"

#include "cmdline.h"
#include "numbers_column.h"
#include "log.h"

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/icontext.h>
#include <coreplugin/icore.h>

#include <gui/input.h>
#include <msgpackrequest.h>
#include <neovimconnector.h>

#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>

#include <texteditor/displaysettings.h>
#include <texteditor/fontsettings.h>
#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>
#include <texteditor/texteditorsettings.h>
#include <texteditor/tabsettings.h>

#include <utils/differ.h>
#include <utils/fancylineedit.h>
#include <utils/fileutils.h>
#include <utils/osspecificaspects.h>
#include <utils/plaintextedit/plaintextedit.h>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QHash>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSplitter>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextOption>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <functional>

namespace QNVim {
namespace Internal {

namespace {

// The editor that is current in an editor view of Qt Creator's editor area
// (each view shows one editor at a time).
Core::IEditor *editorForView(QWidget *view) {
    if (!view)
        return nullptr;

    for (auto editor : Core::EditorManager::visibleEditors()) {
        QWidget *editorWidget = editor ? editor->widget() : nullptr;
        QWidget *stack = editorWidget ? editorWidget->parentWidget() : nullptr;
        if (stack and stack->parentWidget() == view)
            return editor;
    }

    return nullptr;
}

} // namespace

QNVimCore::QNVimCore(QObject *parent)
    : QObject{parent} {
    qDebug(Main) << "QNVimCore::constructor";

    saveCursorFlashTime(QApplication::cursorFlashTime());

    connect(Core::EditorManager::instance(), &Core::EditorManager::editorAboutToClose,
            this, &QNVimCore::editorAboutToClose);
    connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged,
            this, &QNVimCore::editorOpened);

    // Neovim's 'wrap' option and Qt Creator's "Enable text wrapping" setting
    // are kept in sync, so that both describe the same editors. Neovim's own
    // value is applied to the editors (and the setting) as soon as it is read
    // for the first time; reading the setting here makes sure it was loaded
    // from Qt Creator's settings before the connection, so loading cannot
    // look like a change later.
    mWrapSetting = TextEditor::displaySettings().textWrapping();
    connect(&TextEditor::displaySettings(), &TextEditor::DisplaySettings::changed,
            this, &QNVimCore::wrapSettingChanged);

    mNumbersColumn = new NumbersColumn();
    // Prevent inheriting parent Neovim's socket when Qt Creator is launched from inside Neovim
    // (e.g. via :terminal or opencode running inside nvim). The child nvim would otherwise
    // inherit NVIM_LISTEN_ADDRESS and hang on `e` / `nvim_input`.
    QByteArray oldNvim = qgetenv("NVIM");
    QByteArray oldListen = qgetenv("NVIM_LISTEN_ADDRESS");
    qunsetenv("NVIM");
    qunsetenv("NVIM_LISTEN_ADDRESS");
    QStringList nvimArgs{"--cmd", "let g:QNVIM=1"};
    // Dev hook: QNVIM_LISTEN=<path> also makes the spawned Neovim listen on that
    // socket, so its own view can be inspected while qnvim runs, e.g.
    //   nvim --server <path> --remote-expr 'line(".")'
    const QByteArray listenAddress = qgetenv("QNVIM_LISTEN");
    if (!listenAddress.isEmpty())
        nvimArgs << "--listen" << QString::fromLocal8Bit(listenAddress);
    mNVim = NeovimQt::NeovimConnector::spawn(nvimArgs);
    if (!oldNvim.isEmpty())
        qputenv("NVIM", oldNvim);
    if (!oldListen.isEmpty())
        qputenv("NVIM_LISTEN_ADDRESS", oldListen);

    connect(mNVim, &NeovimQt::NeovimConnector::error, this, [this]() {
        qCritical(Main) << "Neovim:" << mNVim->errorString() << "- QNVim functionality is unavailable";
        // The overlays of Neovim's floating windows have nothing to show any
        // more.
        clearFloats();
    });

    connect(mNVim, &NeovimQt::NeovimConnector::ready, this, &QNVimCore::setupNeovim);

    m_cmdLine = new CmdLine(this);
}

QNVimCore::~QNVimCore()
{
    disconnect(QApplication::styleHints(), &QStyleHints::cursorFlashTimeChanged,
               this, &QNVimCore::saveCursorFlashTime);
    QApplication::setCursorFlashTime(mSavedCursorFlashTime);

    mNumbersColumn->deleteLater();

    // The overlays of Neovim's floating windows live in Qt Creator's main
    // window; drop them before anything else goes away.
    clearFloats();
    if (mNVim and mNVim->isReady()) {
        auto request = mNVim->api2()->nvim_command("q!");
        connect(request, &NeovimQt::MsgpackRequest::finished, this, [=, this]() {
            mNVim->deleteLater();
            mNVim = nullptr;
        });
    } else if (mNVim) {
        mNVim->deleteLater();
        mNVim = nullptr;
    }
    disconnect(Core::EditorManager::instance(), &Core::EditorManager::editorAboutToClose,
               this, &QNVimCore::editorAboutToClose);
    disconnect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged,
               this, &QNVimCore::editorOpened);
    const auto keys = mEditors.keys();
    for (const auto key : keys) {
        Core::IEditor *editor = mEditors[key];
        if (!editor)
            continue;

        QWidget *widget = editor->widget();
        if (!widget)
            continue;

        if (!qobject_cast<TextEditor::TextEditorWidget *>(widget))
            continue;

        auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(widget);
        textEditor->setCursorWidth(1);
        widget->removeEventFilter(this);
        mEditors.remove(key);
    }
    mBuffers.clear();
    mChangedTicks.clear();
    mBufferType.clear();

    if (mNVim)
        mNVim->deleteLater();
}

void QNVimCore::setupNeovim() {
    sendPreamble();
    attachUi();
    installGuiHooks();
}

// The preamble defines the commands and autocommands that make Neovim notify
// qnvim through the channel; Neovim gets the channel number as the first line
// of what is sent, so the rest of the preamble can call rpcnotify() with it.
void QNVimCore::sendPreamble() {
    QFile preamble(QStringLiteral(":/qnvim/preamble.vim"));
    if (!preamble.open(QFile::ReadOnly | QFile::Text)) {
        qWarning(Main) << "Could not read the VimL preamble:" << preamble.errorString();
        return;
    }

    QByteArray setupCommand = "let g:neovim_channel=";
    setupCommand += QByteArray::number(static_cast<qulonglong>(mNVim->channel()));
    setupCommand += "\n";
    setupCommand += preamble.readAll();

    auto setupRequest = mNVim->api2()->nvim_command(setupCommand);
    connect(setupRequest, &NeovimQt::MsgpackRequest::error, this,
            [](quint32, quint64, const QVariant &error) {
        qWarning(Main) << "Could not set up Neovim:" << error;
    });
}

// The ext_* options decide which events Neovim sends (the redraw events qnvim
// renders). Attaching is what makes Neovim draw at all, so the editor the user
// is in is opened once the attachment is done.
void QNVimCore::attachUi() {
    QVariantMap options;
    options.insert("ext_popupmenu", true);
    options.insert("ext_tabline", false);
    options.insert("ext_cmdline", true);
    options.insert("ext_wildmenu", true);
    options.insert("ext_messages", true);
    options.insert("ext_multigrid", true);
    options.insert("ext_hlstate", true);
    options.insert("rgb", true);
    NeovimQt::MsgpackRequest *request = mNVim->api2()->nvim_ui_attach(mWidth, mHeight, options);
    request->setTimeout(10000);
    connect(request, &NeovimQt::MsgpackRequest::timeout, mNVim, &NeovimQt::NeovimConnector::fatalTimeout);
    connect(request, &NeovimQt::MsgpackRequest::timeout, [=, this]() {
        qCritical(Main) << "Neovim: Connection timed out!";
    });
    connect(request, &NeovimQt::MsgpackRequest::finished, this, [=, this]() {
        qInfo(Main) << "Neovim: attached!";

        auto pCurrentEditor = Core::EditorManager::currentEditor();
        if (pCurrentEditor)
            QNVimCore::editorOpened(pCurrentEditor);
    });
}

// The autocommands of the preamble and the buffer updates arrive as
// notifications (see handleNotification()); qnvim has to subscribe to them
// before Neovim sends any.
void QNVimCore::installGuiHooks() {
    connect(mNVim->api2(), &NeovimQt::NeovimApi2::neovimNotification,
            this, &QNVimCore::handleNotification);

    mNVim->api2()->nvim_subscribe("Gui");
    mNVim->api2()->nvim_subscribe("api-buffer-updates");
}

QString QNVimCore::filename(Core::IEditor *editor) const {
    if (!editor)
        return QString();

    auto filename = editor->document()->filePath().toFSPathString();
    if (filename.isEmpty())
        filename = editor->document()->displayName();

    return filename;
}

// The text columns the editor shows in its viewport. Qt Creator wraps its
// lines in PlainTextDocumentLayout::layoutBlock(), which lays the text out in
// the viewport width minus the document margin on both sides and minus one
// more character: the editor always enables the "add space for line and
// paragraph separators" option, which reserves room for the paragraph mark.
// Neovim has to wrap at the same columns, or the rows of its window are not
// the rows of the editor (the cursor then lands off the row it is on).
static int textColumns(const TextEditor::TextEditorWidget *textEditor) {
    const QTextDocument *document = textEditor->document();
    QFontMetricsF fm(textEditor->textDocument()->fontSettings().font());
    const qreal advance = fm.horizontalAdvance('A');
    if (advance <= 0)
        return 0;

    qreal width = textEditor->viewport()->width() - 2 * document->documentMargin();
    if (document->defaultTextOption().flags() & QTextOption::AddSpaceForLineAndParagraphSeparators)
        width -= fm.horizontalAdvance(QChar(0x21B5));

    // Qt fits characters with 1/64th pixel precision (QFixed); the same
    // arithmetic gives the same count at the boundaries.
    const int cell = qRound(advance * 64);
    if (cell <= 0)
        return 0;

    return qMax(0, qRound(width * 64)) / cell;
}

// Sets how an editor wraps its lines: with Neovim's 'wrap' on it wraps at any
// character (Neovim breaks at the cell it runs out of room at, not at a word
// boundary), with 'wrap' off it does not wrap at all.
//
// Qt keeps the mode in two places: the editor widget (QPlainTextEdit::
// wordWrapMode()) and the text document, and the layout wraps by the document's
// mode. Editors of one document (the halves of a Qt Creator split, ...) share
// the document, and a new editor for a document resets the document's mode to
// Qt's default (break at word boundaries) when it attaches to it. The widget's
// own value then says nothing about the mode that is in effect, so both have to
// be set.
static void applyWrapMode(Core::IEditor *editor, bool wrap) {
    if (!editor)
        return;

    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    if (!textEditor)
        return;

    const QTextOption::WrapMode mode = wrap ? QTextOption::WrapAnywhere : QTextOption::NoWrap;

    QTextDocument *document = textEditor->document();

    textEditor->setLineWrapMode(wrap ? Utils::PlainTextEdit::WidgetWidth
                                     : Utils::PlainTextEdit::NoWrap);

    if (textEditor->wordWrapMode() != mode)
        textEditor->setWordWrapMode(mode); // also sets the document's mode

    if (document->defaultTextOption().wrapMode() != mode) {
        QTextOption option = document->defaultTextOption();
        option.setWrapMode(mode);
        document->setDefaultTextOption(option);
    }
}

void QNVimCore::fixSize(Core::IEditor *editor) {
    if (!editor) {
        return;
    }

    if (!mNVim or !mNVim->isReady())
        return;

    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    if (!textEditor)
        return;

    QFontMetricsF fm(textEditor->textDocument()->fontSettings().font());

    // The screen (grid 1) is the whole main window: Neovim positions and sizes
    // the floats it draws (pickers, popups, ...) in its cells, and they belong
    // to the window, not to the text area of one editor. The editors themselves
    // keep Neovim's wrapping: every window's grid is sized to its view below.
    if (auto mainWindow = Core::ICore::mainWindow()) {
        const int width = qFloor(mainWindow->width() / fm.horizontalAdvance('A'));
        const int height = qFloor(mainWindow->height() / fm.lineSpacing());
        if (width > 0 and height > 0 and (width != mWidth or height != mHeight))
            mNVim->api6()->nvim_ui_try_resize_grid(1, width, height);
    }

    // The window the editor is shown in must wrap its lines where the editor
    // wraps them; the size of the window's own grid decides that.
    const int win = windowForEditor(editor);
    auto windowState = mWindows.find(win);
    if (!win)
        return;

    if (windowState == mWindows.end() or windowState.value().textoff < 0)
        refreshWindowSizes(); // learns the external columns, then sizes the grid
    else
        resizeWindow(win);
}

void QNVimCore::resizeWindow(int win) {
    auto it = mWindows.find(win);
    if (it == mWindows.end())
        return;

    WindowState &st = *it;
    if (!st.bound or !st.editor or !st.grid or !mNVim or !mNVim->isReady())
        return;

    if (st.textoff < 0)
        return; // the grid cannot be sized correctly before the columns are known

    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(st.editor->widget());
    if (!textEditor)
        return;

    QFontMetricsF fm(textEditor->textDocument()->fontSettings().font());

    // The grid holds the text columns of the editor (Neovim wraps the window
    // at those) plus the window's external columns (line numbers, sign and
    // fold columns, 'statuscolumn'), which are not part of the text.
    const int columns = textColumns(textEditor);
    const int rows = qFloor(textEditor->height() / fm.lineSpacing());
    const int width = columns + st.textoff;

    if (columns <= 0 or rows <= 0 or (width == st.gridWidth and rows == st.gridHeight))
        return;

    st.gridWidth = width;
    st.gridHeight = rows;
    mNVim->api6()->nvim_ui_try_resize_grid(st.grid, width, rows);
}

void QNVimCore::refreshWindowSizes() {
    if (!mNVim or !mNVim->isReady() or mRefreshingWindowSizes)
        return;

    mRefreshingWindowSizes = true;
    // getwininfo() gives each window's external columns ('textoff'), its winbar
    // rows and its number of lines; 'wrap' has to come with them so that every
    // window's editor can wrap as its window does (an editor that was never the
    // current one has not heard from syncFromVim() yet).
    auto request = mNVim->api2()->nvim_eval(
        QByteArrayLiteral("map(getwininfo(), {_, w -> extend(w, "
                          "{'qnvim_wrap': getwinvar(w.winid, '&wrap'), "
                          "'qnvim_winbar': get(w, 'winbar', 0)})})"));
    connect(request, &NeovimQt::MsgpackRequest::error, this,
            [=, this](quint32, quint64, const QVariant &error) {
        mRefreshingWindowSizes = false;
        qWarning(Main) << "Could not read the size of the Neovim windows:" << error;
    });
    connect(request, &NeovimQt::MsgpackRequest::finished, this,
            [=, this](quint32, quint64, const QVariant &result) {
        // The external columns of a window are part of its grid: without them
        // the grid would be too narrow and Neovim would wrap too early (the
        // window's text must fill its editor view).
        for (const QVariant &entry : result.toList()) {
            const QVariantMap info = entry.toMap();
            auto windowState = mWindows.find(info.value("winid").toInt());
            if (windowState != mWindows.end()) {
                windowState.value().textoff = info.value("textoff").toInt();
                windowState.value().winbar = info.value("qnvim_winbar").toInt();
                if (windowState.value().bound)
                    applyWrapMode(windowState.value().editor, info.value("qnvim_wrap").toBool());
            }
        }

        for (auto it = mWindows.cbegin(); it != mWindows.cend(); ++it) {
            if (it.value().bound)
                resizeWindow(it.key());
        }

        mRefreshingWindowSizes = false;
    });
}

int QNVimCore::bufferForEditor(Core::IEditor *editor) const {
    if (!editor)
        return 0;

    if (mBuffers.contains(editor))
        return mBuffers[editor];

    // Another editor may show the same document (a Qt Creator split shows its
    // own editor for the document); a document has a single Neovim buffer.
    Core::IDocument *document = editor->document();
    for (auto it = mBuffers.cbegin(); it != mBuffers.cend(); ++it) {
        Core::IEditor *other = it.key();
        if (other and other != editor and other->document() == document)
            return it.value();
    }

    return 0;
}

QWidget *QNVimCore::editorViewWidget(Core::IEditor *editor) const {
    QWidget *widget = editor ? editor->widget() : nullptr;
    QWidget *stack = widget ? widget->parentWidget() : nullptr;
    return stack ? stack->parentWidget() : nullptr;
}

Core::IEditor *QNVimCore::editorForWidget(QObject *object) const {
    if (!object)
        return nullptr;

    QWidget *widget = qobject_cast<QWidget *>(object);
    for (Core::IEditor *editor : mBuffers.keys()) {
        QWidget *editorWidget = editor ? editor->widget() : nullptr;
        if (!editorWidget)
            continue;

        if (editorWidget == widget or editorWidget->isAncestorOf(widget))
            return editor;
    }

    return nullptr;
}

int QNVimCore::windowForView(QWidget *view) const {
    if (!view)
        return 0;

    for (auto it = mWindows.cbegin(); it != mWindows.cend(); ++it) {
        if (it.value().view == view)
            return it.key();
    }

    return 0;
}

int QNVimCore::windowForEditor(Core::IEditor *editor) const {
    return windowForView(editorViewWidget(editor));
}

int QNVimCore::windowWithoutView(int buffer) const {
    if (!buffer)
        return 0;

    for (auto it = mWindows.cbegin(); it != mWindows.cend(); ++it) {
        const WindowState &st = it.value();
        if (!st.bound and st.buffer == buffer)
            return it.key();
    }

    return 0;
}

Core::IEditor *QNVimCore::editorWithoutWindow(int buffer) const {
    if (!buffer)
        return nullptr;

    for (auto it = mBuffers.cbegin(); it != mBuffers.cend(); ++it) {
        Core::IEditor *editor = it.key();
        if (it.value() != buffer or !editor)
            continue;

        QWidget *view = editorViewWidget(editor);
        if (view and !windowForView(view))
            return editor;
    }

    return nullptr;
}

void QNVimCore::bindWindow(int win, Core::IEditor *editor) {
    if (!win or !editor)
        return;

    WindowState &st = mWindows[win];
    st.bound = true;
    st.editor = editor;
    st.view = editorViewWidget(editor);
    if (const int buffer = bufferForEditor(editor))
        st.buffer = buffer;
    st.cursorLine = st.cursorCol = st.vCursorLine = st.vCursorCol = 0;
    st.topline = st.pendingTopline = 0;
    mNewWindows.remove(win);

    // The view the window is represented by decides how wide the window's text
    // is; a window not seen by qnvim before has to be asked Neovim for its
    // external columns first (see resizeWindow()).
    if (st.grid and st.textoff < 0)
        refreshWindowSizes();
    else
        resizeWindow(win);
}

void QNVimCore::activateWindowEditor(int win) {
    auto it = mWindows.find(win);
    if (it == mWindows.end())
        return;

    WindowState &st = *it;
    Core::IEditor *editor = st.editor;
    if (!editor or editorViewWidget(editor) != st.view) {
        // The view may show another editor now (the stored one was closed, or
        // moved to another view, as Qt Creator does when a split is removed);
        // the window represents the editor its view shows.
        editor = editorForView(st.view);
        st.editor = editor;
    }

    if (!editor or Core::EditorManager::currentEditor() == editor)
        return;

    Core::EditorManager::activateEditor(editor);
}

void QNVimCore::syncWindow(int win, Core::IEditor *editor) {
    auto it = mWindows.find(win);
    if (it == mWindows.end() or !editor)
        return;

    const int buffer = bufferForEditor(editor);
    if (!buffer)
        return;

    WindowState &st = *it;
    const bool bufferChanged = (st.buffer != buffer);
    st.editor = editor;
    st.view = editorViewWidget(editor);
    st.buffer = buffer;

    if (bufferChanged) {
        // The window shows another buffer now; nothing was synced for it yet.
        st.cursorLine = st.cursorCol = st.vCursorLine = st.vCursorCol = 0;
        st.topline = st.pendingTopline = 0;
    }

    if (!mNVim or !mNVim->isReady() or mInhibitWindowSync)
        return;

    if (bufferChanged)
        mNVim->api5()->nvim_win_set_buf(win, buffer);

    const bool currentChanged = (win != mCurrentWindow);
    if (currentChanged) {
        mCurrentWindow = win;
        mNVim->api2()->nvim_set_current_win(win);
    }

    if (bufferChanged or currentChanged)
        syncCursorToVim(editor);
}

void QNVimCore::updateWindowBuffer(int win, int buffer, const QString &filename,
                                   const QString &bufferType, bool alwaysText) {
    auto it = mWindows.find(win);
    if (it == mWindows.end())
        return;

    WindowState &st = *it;
    st.buffer = buffer;
    st.cursorLine = st.cursorCol = st.vCursorLine = st.vCursorCol = 0;
    st.topline = st.pendingTopline = 0;
    // The buffer was entered in this window: unless a command moved the
    // window, this is the window Neovim works in now (`:e` does not fire
    // WinEnter).
    mCurrentWindow = win;

    // The view of the window may already show the buffer: `e filename` in
    // editorOpened() opened the file in Neovim, and this is its BufEnter.
    Core::IEditor *shown = editorForView(st.view);
    if (shown and !filename.isEmpty() and filename == this->filename(shown)) {
        st.editor = shown;
        return;
    }

    const bool inhibited = mInhibitWindowSync;
    mInhibitWindowSync = true;
    // The window's view is where the buffer has to be shown.
    activateWindowEditor(win);
    Core::IEditor *editor = openEditorForBuffer(buffer, filename, bufferType, alwaysText);
    mInhibitWindowSync = inhibited;

    if (editor)
        syncWindow(win, editor);
}

void QNVimCore::ensureWindowForView(Core::IEditor *editor) {
    if (!mNVim or !mNVim->isReady() or mInhibitWindowSync)
        return;

    if (!editor)
        return;

    QWidget *view = editorViewWidget(editor);
    if (!view or windowForView(view))
        return;

    if (mCreatingWindowForView == view)
        return; // the window is already being created for this view

    const int buffer = bufferForEditor(editor);
    if (!buffer) {
        // The buffer is not known yet (the editor is still being opened in
        // Neovim); its window is created once it is (see reconcileWindows()).
        mWindowsDirty = true;
        return;
    }

    mCreatingWindowForView = view;

    // The orientation of the nearest splitter says where the view is in Qt
    // Creator: a view side by side with the others gets a window next to the
    // current one, a stacked view one below it.
    QByteArray split = "below";
    for (QWidget *widget = view; widget; widget = widget->parentWidget()) {
        if (auto splitter = qobject_cast<QSplitter *>(widget)) {
            if (splitter->orientation() == Qt::Horizontal)
                split = "right";
            break;
        }
    }

    QVariantMap config;
    config.insert("split", split);
    if (mCurrentWindow)
        config.insert("win", mCurrentWindow);

    auto request = mNVim->api6()->nvim_open_win(buffer, true, config);
    connect(request, &NeovimQt::MsgpackRequest::finished, this,
            [=, this](quint32, quint64, const QVariant &result) {
        mCreatingWindowForView = nullptr;

        const int win = result.toInt();
        if (!win)
            return;

        auto it = mWindows.find(win);
        if (it != mWindows.end() and it.value().bound)
            return; // paired with a view already (e.g. by reconcileWindows())

        // The editor the view shows now is the one the window represents; the
        // editor the window was created for may be gone by now.
        Core::IEditor *windowEditor = editorForView(view);
        if (!windowEditor and mBuffers.contains(editor))
            windowEditor = editor;
        if (!windowEditor) {
            mWindowsDirty = true;
            return;
        }

        bindWindow(win, windowEditor);
        syncCursorToVim(windowEditor);
    });
    connect(request, &NeovimQt::MsgpackRequest::error, this,
            [=, this](quint32, quint64, const QVariant &error) {
        qWarning(Main) << "Could not open a window in Neovim:" << error;
        mCreatingWindowForView = nullptr;
    });
}

void QNVimCore::createViewForWindow(int win, int buffer, const QString &filename, int sourceWin,
                                    bool sideBySide, bool before) {
    if (!mNVim or !mNVim->isReady())
        return;

    auto it = mWindows.find(win);
    if (it == mWindows.end() or it.value().bound)
        return;

    mCreatingViewForWindow = win;
    const bool inhibited = mInhibitWindowSync;
    mInhibitWindowSync = true;

    // The window was split off the source window: create the view next to the
    // view of the source window. Qt Creator creates the new view after (right
    // of / below) the view it splits, so a window that Neovim placed before
    // (left of / above) its source window is given the source view instead
    // (see below).
    Core::IEditor *sourceEditor = nullptr;
    if (sourceWin) {
        activateWindowEditor(sourceWin);
        if (mWindows.contains(sourceWin))
            sourceEditor = mWindows[sourceWin].editor;
    }

    if (sideBySide)
        Core::EditorManager::splitSideBySide();
    else
        Core::EditorManager::split();

    // The split duplicated the editor of the source window into the new view.
    // That duplicate is the editor of the new window as long as it shows its
    // buffer. Opening the buffer instead would activate an editor from another
    // view, and Qt Creator moves such an editor into the new view (it only
    // duplicates an editor that is visible where it is).
    Core::IEditor *editor = Core::EditorManager::currentEditor();
    const bool duplicated = editor and bufferForEditor(editor) == buffer;
    if (!duplicated)
        editor = openEditorForBuffer(buffer, filename, mBufferType.value(buffer), mAlwaysText);

    mInhibitWindowSync = inhibited;
    mCreatingViewForWindow = 0;

    if (before and duplicated and sourceWin and sourceEditor and editor
        and mWindows.contains(sourceWin)) {
        // Neovim placed the new window before the source window, but Qt
        // Creator only splits after it: the two views are the right pair, so
        // the windows are swapped between them.
        bindWindow(win, sourceEditor);
        bindWindow(sourceWin, editor);
        // Neovim works in the new window; show its (left/top) view.
        activateWindowEditor(win);
    } else if (editor and mWindows.contains(win) and !mWindows[win].bound) {
        bindWindow(win, editor);
    }

    // If no editor could be created for the buffer (e.g. no editor factory
    // accepts the file), the window stays without a view; it is paired with
    // one once an editor that shows its buffer is opened. Retrying the view
    // creation would only add empty splits.
    mNewWindows.remove(win);
}

Core::IEditor *QNVimCore::openEditorForBuffer(int buffer, const QString &filename,
                                              const QString &bufferType, bool alwaysText) {
    Core::IEditor *editor = mEditors.value(buffer);
    if (editor) {
        // An editor for the buffer exists (in some view): show it in the view
        // the user is working in (Qt Creator duplicates it there if it can).
        Core::EditorManager::activateEditorForDocument(editor->document());
        Core::IEditor *current = Core::EditorManager::currentEditor();
        if (current and current->document() == editor->document())
            return current;
        return editor;
    }

    if (filename.isEmpty())
        return nullptr;

    // The editor is created from a Neovim autocommand: tell editorOpened()
    // which buffer it is for, so that the content comes from Qt Creator
    // instead of Neovim reading the file.
    const int settingBufferFromVim = mSettingBufferFromVim;
    mSettingBufferFromVim = buffer;

    Core::IEditor *opened = nullptr;
    if (bufferType.isEmpty() and QFile::exists(filename)) {
        QFileInfo fileInfo = QFileInfo(filename);
        if (alwaysText) {
            if ((QStringList() << "js"
                               << "qml"
                               << "cpp"
                               << "c"
                               << "cc"
                               << "hpp"
                               << "h"
                               << "pro")
                    .contains(fileInfo.suffix(), Qt::CaseInsensitive))
                opened = Core::EditorManager::openEditor(Utils::FilePath::fromString(filename));
            else
                opened = Core::EditorManager::openEditor(Utils::FilePath::fromString(filename),
                                                         "Core.PlainTextEditor");
        } else {
            opened = Core::EditorManager::openEditor(Utils::FilePath::fromString(filename));
        }
    } else if (bufferType == "help") {
        QString title = filename;
        opened = Core::EditorManager::openEditorWithContents("Help", &title, QByteArray(), filename);
        if (opened) {
            opened->document()->setFilePath(Utils::FilePath::fromString(filename));
            opened->document()->setPreferredDisplayName(filename);
        }
    } else {
        QString title = filename;
        opened = Core::EditorManager::openEditorWithContents("Terminal", &title, QByteArray(),
                                                             filename);
    }

    mSettingBufferFromVim = settingBufferFromVim;
    return opened;
}

void QNVimCore::closeWindowForView(int win) {
    if (!mNVim or !mNVim->isReady())
        return;

    if (!mWindows.contains(win))
        return;

    // Neovim is told to close the window; no Qt Creator split must be removed
    // when Neovim reports it closed.
    mClosingWindows.insert(win);
    mNewWindows.remove(win);

    auto request = mNVim->api6()->nvim_win_close(win, true);
    connect(request, &NeovimQt::MsgpackRequest::error, this,
            [=, this](quint32, quint64, const QVariant &error) {
        qWarning(Main) << "Could not close a window in Neovim:" << error;
        mClosingWindows.remove(win);
        auto it = mWindows.find(win);
        if (it == mWindows.end())
            return; // the window is gone (WinClosed() already took care of it)

        // The window still exists (Neovim cannot close its last one): it no
        // longer represents the view the close was requested for, but it may
        // be paired with a view again.
        it.value().bound = false;
        it.value().editor = nullptr;
        it.value().view = nullptr;
    });
}

void QNVimCore::setupEditor(Core::IEditor *editor) {
    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    if (!textEditor)
        return;

    Core::IDocument *document = editor->document();

    connect(document, &Core::IDocument::contentsChanged, this, [=, this]() {
            auto buffer = mBuffers.value(editor);
            QString bufferType = mBufferType[buffer];
            if (!mEditors.contains(buffer) or (bufferType != "acwrite" and !bufferType.isEmpty()))
                return;
            syncToVim(editor);
        },
        Qt::QueuedConnection);
    connect(textEditor, &TextEditor::TextEditorWidget::cursorPositionChanged, this, [=, this]() {
            if (Core::EditorManager::currentEditor() != editor)
                return;
            // Cursor movements while the editor content is not in sync with
            // Neovim (e.g. a locally changed document is still being pushed)
            // must not be sent.
            if (mBuffers.value(editor) != mTextBuffer)
                return;
            syncCursorToVim(editor);
        },
        Qt::QueuedConnection);
    connect(textEditor, &TextEditor::TextEditorWidget::selectionChanged, this, [=, this]() {
            if (Core::EditorManager::currentEditor() != editor)
                return;
            if (mBuffers.value(editor) != mTextBuffer)
                return;
            syncSelectionToVim(editor);
        },
        Qt::QueuedConnection);
    connect(textEditor->textDocument(), &TextEditor::TextDocument::fontSettingsChanged,
            this, &QNVimCore::updateCursorSize);
}

void QNVimCore::closeWindowsForRemovedViews() {
    if (!mNVim or !mNVim->isReady())
        return;

    // A window whose editor view is gone (the user removed the split in
    // Qt Creator) has no counterpart anymore. Neovim cannot close its last
    // window, so the last one is left alone.
    if (mWindows.size() <= 1)
        return;

    for (auto it = mWindows.cbegin(); it != mWindows.cend(); ++it) {
        const WindowState &st = it.value();
        if (!st.bound or st.view or mClosingWindows.contains(it.key()))
            continue;

        closeWindowForView(it.key());
    }
}

void QNVimCore::reconcileWindows() {
    if (!mWindowsDirty or !mNVim or !mNVim->isReady())
        return;

    // Do not ask again while the request is in flight; if it fails, the
    // windows are reconciled on the next redraw flush.
    mWindowsDirty = false;

    auto request = mNVim->api2()->nvim_call_function(QByteArrayLiteral("QNVimWindowInfo"), {});
    connect(request, &NeovimQt::MsgpackRequest::error, this,
            [this](quint32, quint64, const QVariant &error) {
        qWarning(Main) << "Could not read the windows from Neovim:" << error;
        mWindowsDirty = true;
    });
    connect(request, &NeovimQt::MsgpackRequest::finished, this,
            [=, this](quint32, quint64, const QVariant &result) {
        struct WindowInfo {
            int win = 0;
            int buffer = 0;
            QString filename;
            int row = 0;
            int col = 0;
            int tab = 0;
        };
        QList<WindowInfo> windows;

        for (const QVariant &entry : result.toList()) {
            const QVariantList info = entry.toList();
            if (info.size() < 9)
                continue;

            WindowInfo window;
            window.win = info[0].toInt();
            window.buffer = info[1].toInt();
            window.filename = info[2].toString();
            window.row = info[3].toInt();
            window.col = info[4].toInt();
            window.tab = info[7].toInt();
            if (!window.win)
                continue;

            if (info[8].toBool())
                mCurrentWindow = window.win;
            if (!mWindows.contains(window.win))
                mWindows[window.win].buffer = window.buffer; // e.g. Neovim's first window

            windows.append(window);
        }

        // Windows Neovim created that are still nothing but windows now that
        // the command that created them was fully processed are splits: each
        // one needs an editor view in Qt Creator.
        QSet<int> newWindows;
        for (const WindowInfo &window : windows) {
            if (!mNewWindows.contains(window.win))
                continue;

            auto it = mWindows.find(window.win);
            if (it == mWindows.end() or it.value().bound)
                continue;

            if (Core::IEditor *editor = editorWithoutWindow(window.buffer)) {
                // An editor for the buffer has no view yet: pair them.
                bindWindow(window.win, editor);
                continue;
            }

            if (mEditors.contains(window.buffer) or !window.filename.isEmpty()) {
                // The view is created next to the view of the window Neovim
                // split this one off: the window the new one is adjacent to in
                // the layout. The new window can be before the window it was
                // split from (left of / above it) as well, e.g. with
                // 'splitright' or 'splitbelow'.
                int sourceWin = 0;
                bool sideBySide = false;
                bool before = false;
                int bestDistance = -1;
                for (const WindowInfo &source : windows) {
                    if (source.win == window.win or source.tab != window.tab)
                        continue;
                    auto sourceIt = mWindows.find(source.win);
                    if (sourceIt == mWindows.end() or !sourceIt.value().bound)
                        continue;

                    bool candidateSideBySide = false;
                    bool candidateBefore = false;
                    int distance = -1;
                    // A window's grid is sized to its editor view (see
                    // resizeWindow()), so its width and height are those of
                    // that view, not of the space it occupies in Neovim's
                    // layout; only its position is the layout's. Windows side
                    // by side in the layout are in the same row and differ in
                    // the column, stacked ones are in the same column and
                    // differ in the row; on either side, the nearest one is
                    // the window the new one is next to.
                    if (source.row == window.row and source.col != window.col) {
                        candidateSideBySide = true;
                        if (source.col < window.col) {
                            // The source is left of the new window.
                            candidateBefore = false;
                            distance = window.col - source.col;
                        } else {
                            // The source is right of the new window.
                            candidateBefore = true;
                            distance = source.col - window.col;
                        }
                    } else if (source.col == window.col and source.row != window.row) {
                        candidateSideBySide = false;
                        if (source.row < window.row) {
                            // The source is above the new window.
                            candidateBefore = false;
                            distance = window.row - source.row;
                        } else {
                            // The source is below the new window.
                            candidateBefore = true;
                            distance = source.row - window.row;
                        }
                    } else {
                        continue;
                    }

                    if (bestDistance < 0 or distance < bestDistance) {
                        bestDistance = distance;
                        sourceWin = source.win;
                        sideBySide = candidateSideBySide;
                        before = candidateBefore;
                    }
                }

                createViewForWindow(window.win, window.buffer, window.filename, sourceWin,
                                    sideBySide, before);
            } else {
                // No editor and no file name: there is nothing to show in
                // Qt Creator yet; the window is handled once it has a buffer
                // with a file name.
                newWindows.insert(window.win);
            }
        }

        mNewWindows = newWindows;
    });
}

void QNVimCore::attachEditorToWindow(Core::IEditor *editor) {
    if (!editor)
        return;

    const QPointer<QWidget> view = editorViewWidget(editor);

    // The editor's view may already represent a window: it shows the editor
    // now.
    if (const int win = windowForView(view)) {
        syncWindow(win, editor);
        return;
    }

    if (mRemovingView)
        return; // the view of the editor is being removed

    if (mCreatingViewForWindow) {
        // Qt Creator is being made to show a view for the window (the split
        // duplicated the source view's editor): the editor that is meant for
        // that window is the one showing its buffer. The others get a window
        // the regular way, should they ever become current.
        auto it = mWindows.find(mCreatingViewForWindow);
        if (it != mWindows.end() and !it.value().bound
            and bufferForEditor(editor) == it.value().buffer)
            bindWindow(mCreatingViewForWindow, editor);
        return;
    }

    const int buffer = bufferForEditor(editor);
    if (!buffer) {
        // The buffer is not known yet (the editor is still being opened in
        // Neovim); the editor and its window are paired once it is (the next
        // time the windows are reconciled).
        mWindowsDirty = true;
        return;
    }

    // A window that Neovim created for the buffer has no view yet.
    if (const int win = windowWithoutView(buffer)) {
        bindWindow(win, editor);
        return;
    }

    if (mCreatingWindowForView == view)
        return; // the window is already being created for this view

    if (!mNVim or !mNVim->isReady())
        return; // the window is paired once the connection is ready again

    // The buffer may be shown in Neovim's current window already (e.g. the
    // window Neovim had before qnvim could see it): the editor takes it over.
    auto request = mNVim->api2()->nvim_eval(
        QByteArrayLiteral("[win_getid(), bufnr(''), win_gettype()]"));
    connect(request, &NeovimQt::MsgpackRequest::finished, this,
            [=, this](quint32, quint64, const QVariant &result) {
        const QVariantList state = result.toList();
        const int win = state.value(0).toInt();
        const int windowBuffer = state.value(1).toInt();
        const QByteArray windowType = state.value(2).toByteArray();

        if (const int bound = windowForView(view)) {
            syncWindow(bound, editor); // the view has a window by now
            return;
        }

        const int buffer = bufferForEditor(editor);
        if (!buffer)
            return;

        auto it = mWindows.find(win);
        if (win and windowType.isEmpty() and windowBuffer == buffer
            and (it == mWindows.end() or !it.value().bound)) {
            bindWindow(win, editor);
            return;
        }

        ensureWindowForView(editor);
    });
}

void QNVimCore::syncCursorToVim(Core::IEditor *editor) {

    if (!editor)
        editor = Core::EditorManager::currentEditor();

    if (!editor)
        return;

    const int buffer = bufferForEditor(editor);
    const int win = windowForEditor(editor);
    if (!buffer or !win)
        return;

    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    if (!textEditor)
        return;

    if (mMode == "v" or mMode == "V" or mMode == "\x16" or
        textEditor->textCursor().position() != textEditor->textCursor().anchor())
        return;

    const auto text = textEditor->toPlainText();
    int cursorPosition = textEditor->textCursor().position();
    int line = QStringView(text).left(cursorPosition).count('\n') + 1;
    int col = text.left(cursorPosition).section('\n', -1).toUtf8().length() + 1;

    WindowState &st = mWindows[win];
    if (line == st.cursorLine and col == st.cursorCol)
        return;

    st.cursorLine = line;
    st.cursorCol = col;

    if (!mNVim or !mNVim->isReady())
        return;

    if (win == mCurrentWindow)
        mNVim->api2()->nvim_command(QStringLiteral("buffer %1|call SetCursor(%2,%3)").arg(buffer).arg(line).arg(col).toUtf8());
    else
        // A cursor in a window that is not the current one: `buffer N` and
        // friends would affect the current window, set the cursor directly.
        mNVim->api2()->nvim_win_set_cursor(win, QPoint(col - 1, line));
}

void QNVimCore::syncSelectionToVim(Core::IEditor *editor) {
    if (!editor)
        editor = Core::EditorManager::currentEditor();

    if (!editor)
        return;

    const int buffer = bufferForEditor(editor);
    const int win = windowForEditor(editor);
    if (!buffer or !win)
        return;

    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    if (!textEditor)
        return;

    QString text = textEditor->toPlainText();

    auto mtc = textEditor->multiTextCursor();
    int line, col, vLine, vCol;

    QString visualCommand;
    if (mtc.hasMultipleCursors()) {
        auto mainCursor = mtc.mainCursor();

               // We should always use main cursor pos here,
               // because it is the cursor user controls with hjkl
        auto nvimPos = mainCursor.position();

               // NOTE: Theoretically, it is not always the case
               // that the main cursor is at the ends of mtc array,
               // but for creating our own block selections it works,
               // because we create cursors one after another, where
               // main cursor is at the end or in the beginning.
               // @see syncCursorFromVim
        auto lastCursor = mainCursor == *mtc.begin() ? mtc.cursors().last() : *mtc.begin();
        auto nvimAnchor = lastCursor.anchor();

        line = QStringView(text).left(nvimPos).count('\n') + 1;
        col = text.left(nvimPos).section('\n', -1).length() + 1;
        vLine = QStringView(text).left(nvimAnchor).count('\n') + 1;
        vCol = text.left(nvimAnchor).section('\n', -1).length() + 1;

        if (vCol < col)
            --col;
        else if (vCol > col)
            --vCol;

        visualCommand = "\x16";
    } else if (mMode == "V") {
        return;
    } else {
        auto cursor = textEditor->textCursor();
        int cursorPosition = cursor.position();
        int anchorPosition = cursor.anchor();

        if (anchorPosition == cursorPosition)
            return;

        if (anchorPosition < cursorPosition)
            --cursorPosition;
        else
            --anchorPosition;

        line = QStringView(text).left(cursorPosition).count('\n') + 1;
        col = text.left(cursorPosition).section('\n', -1).length() + 1;
        vLine = QStringView(text).left(anchorPosition).count('\n') + 1;
        vCol = text.left(anchorPosition).section('\n', -1).length() + 1;
        visualCommand = "v";
    }

    WindowState &st = mWindows[win];
    if (line == st.cursorLine and col == st.cursorCol and vLine == st.vCursorLine and vCol == st.vCursorCol)
        return;

    st.cursorLine = line;
    st.cursorCol = col;
    st.vCursorLine = vLine;
    st.vCursorCol = vCol;

    if (!mNVim or !mNVim->isReady())
        return;

    if (win == mCurrentWindow)
        mNVim->api2()->nvim_command(QStringLiteral("buffer %1|normal! \x03%3G%4|%2%5G%6|")
                                        .arg(buffer)
                                        .arg(visualCommand)
                                        .arg(vLine)
                                        .arg(vCol)
                                        .arg(line)
                                        .arg(col).toUtf8());
    else
        mNVim->api2()->nvim_win_set_cursor(win, QPoint(col - 1, line));
}

void QNVimCore::syncCursorFromVim(int win, const QVariantList &pos, const QVariantList &vPos, QByteArray mode) {
    const auto winState = mWindows.find(win);
    if (winState == mWindows.end())
        return;

    WindowState &st = *winState;
    const int pendingTopline = st.pendingTopline;
    st.pendingTopline = 0;

    // Each window has its own editor (a view shows one editor); the cursor is
    // applied to it.
    Core::IEditor *editor = st.editor;
    if (!mBuffers.value(editor) or mBuffers.value(editor) != st.buffer or mTextBuffer != st.buffer)
        return;

    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    if (!textEditor)
        return;

    int line = pos[0].toInt();
    int col = pos[1].toInt();
    col = QString::fromUtf8(mText.section('\n', line - 1, line - 1).toUtf8().left(col - 1)).length() + 1;

    int vLine = vPos[0].toInt();
    int vCol = vPos[1].toInt();
    vCol = QString::fromUtf8(mText.section('\n', vLine - 1, vLine - 1).toUtf8().left(vCol)).length();

    mMode = mode;
    st.cursorLine = line;
    st.cursorCol = col;
    st.vCursorLine = vLine;
    st.vCursorCol = vCol;

    // Scroll the editor viewport so that the line at the top matches the top line of
    // Neovim's window. This makes Neovim's viewport related commands (`zz`, `zt`, `zb`,
    // `<C-e>`, `<C-y>`, ...) work.
    if (pendingTopline > 0 and pendingTopline - 1 != textEditor->firstVisibleBlockNumber()) {
        const QTextBlock block = textEditor->document()->findBlockByNumber(pendingTopline - 1);

        if (block.isValid())
            textEditor->setTopBlock(block);
    }

    int anchor = QString("\n" + mText).section('\n', 0, vLine - 1).length() + vCol - 1;
    int position = QString("\n" + mText).section('\n', 0, line - 1).length() + col - 1;
    if (mMode == "V") {
        if (anchor < position) {
            anchor = QString("\n" + mText).section('\n', 0, vLine - 1).length();
            position = QString("\n" + mText).section('\n', 0, line).length() - 1;
        } else {
            anchor = QString("\n" + mText).section('\n', 0, vLine).length() - 1;
            position = QString("\n" + mText).section('\n', 0, line - 1).length();
        }

        QTextCursor cursor = textEditor->textCursor();
        cursor.setPosition(anchor);
        cursor.setPosition(position, QTextCursor::KeepAnchor);

        if (textEditor->textCursor().anchor() != cursor.anchor() or
            textEditor->textCursor().position() != cursor.position())
            textEditor->setTextCursor(cursor);

    } else if (mMode == "v") {
        if (anchor > position)
            ++anchor;
        else
            ++position;

        QTextCursor cursor = textEditor->textCursor();
        cursor.setPosition(anchor);
        cursor.setPosition(position, QTextCursor::KeepAnchor);

        if (textEditor->textCursor().anchor() != cursor.anchor() or
            textEditor->textCursor().position() != cursor.position())
            textEditor->setTextCursor(cursor);
    } else if (mMode == "\x16") { // VISUAL BLOCK
        if (vCol > col)
            ++anchor;
        else
            ++position;

        auto document = textEditor->textCursor().document();
        const auto& tabs = textEditor->textDocument()->tabSettings();

        const auto firstBlock = document->findBlock(anchor);
        const auto lastBlock = document->findBlock(position);
        const auto localAnchor = tabs.columnAt(firstBlock.text(), anchor - firstBlock.position());
        const auto localPos = tabs.columnAt(lastBlock.text(), position - lastBlock.position());

               // Get next block no matter the direction of selection
        auto after = [&](const auto& block) {
            if (anchor < position)
                return block.next();
            else
                return block.previous();
        };

        auto mtc = Utils::MultiTextCursor();
        for (auto curBlock = firstBlock; //
             curBlock.isValid() && curBlock != after(lastBlock); //
             curBlock = after(curBlock)) {

            auto columnsCountInCurBlock = tabs.columnCountForText(curBlock.text());

                   // Skip cursor, if it goes out of the block
            if (columnsCountInCurBlock < localAnchor && columnsCountInCurBlock < localPos)
                continue;

            auto newCursor = QTextCursor(curBlock);

            auto anchorBoundOffset = tabs.positionAtColumn(curBlock.text(), localAnchor);
            auto newCursorAnchor = curBlock.position() + anchorBoundOffset;
            newCursor.setPosition(newCursorAnchor);

            auto posBoundOffset = tabs.positionAtColumn(curBlock.text(), localPos);
            auto newCursorPosition = curBlock.position() + posBoundOffset;
            newCursor.setPosition(newCursorPosition, QTextCursor::KeepAnchor);

            mtc.addCursor(newCursor);
        }
        textEditor->setMultiTextCursor(mtc);
    } else {
        QTextCursor cursor = textEditor->textCursor();

        // The editor's cursor follows Neovim's position exactly, even where the
        // editor draws that position at the end of the previous row: Qt keeps a
        // run of spaces that straddles a wrap point with the previous row, so
        // the first cells of a row Neovim starts with spaces are drawn there.
        cursor.clearSelection();
        cursor.setPosition(position);

        if (textEditor->textCursor().position() != cursor.position() or
            textEditor->textCursor().hasSelection())
            textEditor->setTextCursor(cursor);
    }
}

void QNVimCore::syncToVim(Core::IEditor *editor, std::function<void()> callback) {
    if (!editor)
        editor = Core::EditorManager::currentEditor();

    if (!editor)
        return;

    const int bufferNumber = bufferForEditor(editor);
    if (!bufferNumber)
        return;

    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    if (!textEditor)
        return;

    QString text = textEditor->toPlainText();
    int cursorPosition = textEditor->textCursor().position();
    int line = QStringView(text).left(cursorPosition).count('\n') + 1;
    int col = text.left(cursorPosition).section('\n', -1).toUtf8().length() + 1;

    if (mTextBuffer != bufferNumber or mText != text) {
        auto request = mNVim->api2()->nvim_buf_set_lines(bufferNumber, 0, -1, true, text.toUtf8().split('\n'));
        connect(request, &NeovimQt::MsgpackRequest::finished, this, [=, this]() {
            // The buffer now holds the editor content and may be read back.
            mPendingPush.remove(bufferNumber);
            mText = text;
            mTextBuffer = bufferNumber;

            // The cursor is the current window's business; a window that is not
            // the current one must not be scrolled away from where it is.
            if (Core::EditorManager::currentEditor() != editor or windowForEditor(editor) != mCurrentWindow) {
                if (callback)
                    callback();
                return;
            }

            connect(mNVim->api2()->nvim_command(QStringLiteral("call cursor(%1,%2)").arg(line).arg(col).toUtf8()),
                    &NeovimQt::MsgpackRequest::finished, this, [=, this]() {
                        if (callback)
                            callback();
                    });
        });
    } else if (callback)
        callback();
}

void QNVimCore::wrapSettingChanged() {
    const bool wrap = TextEditor::displaySettings().textWrapping();
    if (wrap == mWrapSetting)
        return; // another display setting changed

    mWrapSetting = wrap;

    if (!mNVim or !mNVim->isReady())
        return;

    // The setting is for all editors, so all Neovim windows follow it (and,
    // through the global value, the ones created later). Each editor follows
    // its own window as usual.
    mNVim->api2()->nvim_command(wrap ? QByteArrayLiteral("setglobal wrap")
                                     : QByteArrayLiteral("setglobal nowrap"));
    mNVim->api2()->nvim_command(wrap ? QByteArrayLiteral("tabdo windo setlocal wrap")
                                     : QByteArrayLiteral("tabdo windo setlocal nowrap"));
}

void QNVimCore::syncFromVim() {
    auto editor = Core::EditorManager::currentEditor();

    if (!editor or !mBuffers.contains(editor))
        return;

    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    if (!textEditor)
        return;

    unsigned long long syncCounter = ++mSyncCounter;

    auto request = mNVim->api2()->nvim_eval(
        "[bufnr(''), win_getid(), b:changedtick, mode(1), &modified, getpos('.'), getpos('v'), &number, &relativenumber, &wrap, getpos('w0'), get(get(getwininfo(win_getid()), 0, {}), 'textoff', -1)]");
    connect(request, &NeovimQt::MsgpackRequest::finished, this, [=, this](quint32, quint64, const QVariant &v) {
        QVariantList state = v.toList();
        if (state.size() < 12)
            return;

        if (mSyncCounter != syncCounter)
            return;

        if (!mBuffers.contains(editor)) {
            return;
        }

        int bufferNumber = state[0].toString().toLong();
        int win = state[1].toInt();
        if (mBuffers[editor] != bufferNumber or !win)
            return;

        unsigned long long changedtick = state[2].toULongLong();
        QByteArray mode = state[3].toByteArray();
        bool modified = state[4].toBool();
        QVariantList pos = state[5].toList().mid(1, 2);
        QVariantList vPos = state[6].toList().mid(1, 2);
        int topline = state[10].toList().value(1).toInt();

        mNumber = state[7].toBool();
        mRelativeNumber = state[8].toBool();
        mWrap = state[9].toBool();
        mNumbersColumn->setNumber(mNumber);
        mNumbersColumn->setEditor(mRelativeNumber ? textEditor : nullptr);

        // Neovim's 'wrap' decides whether the editor wraps its lines and where
        // it breaks them.
        applyWrapMode(editor, mWrap);

        // Neovim's 'wrap' option is shared with Qt Creator's "Enable text
        // wrapping" setting, which follows the window here. Writing it quietly
        // leaves the editors of the other windows alone: they follow their own
        // Neovim windows.
        TextEditor::DisplaySettings &displaySettings = TextEditor::displaySettings();
        if (displaySettings.textWrapping() != mWrap) {
            mWrapSetting = mWrap;
            displaySettings.textWrapping.setValue(mWrap, Utils::BaseAspect::BeQuiet);
        }

        // Neovim's window viewport may have been scrolled without the cursor having moved,
        // e.g. by `zz`, `zt`, `zb`, `<C-e>` or `<C-y>`. Remember the new top line, so that
        // the editor viewport can be scrolled to match once the cursor is synced.
        auto windowState = mWindows.find(win);
        if (windowState != mWindows.end()) {
            WindowState &st = *windowState;
            if (topline > 0 and topline != st.topline) {
                st.topline = topline;
                st.pendingTopline = topline;
            }

            // The window's external columns (line numbers, sign and fold
            // columns, 'statuscolumn') changed, e.g. by `:set number`,
            // `:set signcolumn=yes` or a sign appearing: the window text
            // area no longer fills the editor view, so every window's grid
            // has to be sized to its view again.
            const int textoff = state.value(11).toInt();
            if (textoff >= 0 and st.textoff != textoff) {
                st.textoff = textoff;
                refreshWindowSizes();
            }
        }

        if (mChangedTicks.value(bufferNumber, 0) == changedtick and mTextBuffer == bufferNumber) {
            syncCursorFromVim(win, pos, vPos, mode);
            return;
        }

        mChangedTicks[bufferNumber] = changedtick;

        qDebug(Main) << "QNVimPlugin::syncFromVim";

        auto request = mNVim->api2()->nvim_buf_get_lines(bufferNumber, 0, -1, true);
        connect(request, &NeovimQt::MsgpackRequest::finished, this, [=, this](quint32, quint64, const QVariant &lines) {
            if (!mBuffers.contains(editor) or mBuffers[editor] != bufferNumber) {
                // The editor was closed, or shows another buffer by now.
                return;
            }

            QString text;
            auto linesList = lines.toList();
            for (const auto &t : linesList)
                text += QString::fromUtf8(t.toByteArray()) + '\n';
            text.chop(1);

            QString oldText = textEditor->toPlainText();

            // A buffer that has not received the editor content yet is empty
            // (qnvim suppresses Neovim's own file reading); such a read must not
            // be applied: the editor content may still be loading from disk and
            // must not be wiped by the empty buffer content.
            if (mPendingPush.contains(bufferNumber) and text.isEmpty() and !oldText.isEmpty())
                return;

            // Non-empty content means the buffer holds the editor content by
            // now; from here on the buffer is authoritative and empty reads are
            // genuine (Neovim can empty the buffer, e.g. `dd` or `u`).
            if (!text.isEmpty())
                mPendingPush.remove(bufferNumber);

            mText = text;
            mTextBuffer = bufferNumber;

            Utils::Differ differ;
            auto diff = differ.diff(oldText, mText);

            if (diff.size()) {
                // Update changed lines and keep track of the cursor position
                QTextCursor cursor = textEditor->textCursor();
                int charactersInfrontOfCursor = cursor.position();
                int newCursorPos = charactersInfrontOfCursor;
                cursor.beginEditBlock();
                cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);

                for (const auto &d : diff) {
                    switch (d.command) {
                    case Utils::Diff::Insert: {
                        // Adjust cursor position if we do work in front of the cursor.
                        if (charactersInfrontOfCursor > 0) {
                            const int size = d.text.size();
                            charactersInfrontOfCursor += size;
                            newCursorPos += size;
                        }
                        cursor.insertText(d.text);
                        break;
                    }

                    case Utils::Diff::Delete: {
                        // Adjust cursor position if we do work in front of the cursor.
                        if (charactersInfrontOfCursor > 0) {
                            const int size = d.text.size();
                            charactersInfrontOfCursor -= size;
                            newCursorPos -= size;
                            // Cursor was inside the deleted text, so adjust the new cursor position
                            if (charactersInfrontOfCursor < 0)
                                newCursorPos -= charactersInfrontOfCursor;
                        }
                        cursor.setPosition(cursor.position() + d.text.length(), QTextCursor::KeepAnchor);
                        cursor.removeSelectedText();
                        break;
                    }

                    case Utils::Diff::Equal:
                        // Adjust cursor position
                        charactersInfrontOfCursor -= d.text.size();
                        cursor.setPosition(cursor.position() + d.text.length(), QTextCursor::MoveAnchor);
                        break;
                    }
                }
                cursor.endEditBlock();
                cursor.setPosition(newCursorPos);
            }

            if (textEditor->document()->isModified() != modified)
                textEditor->document()->setModified(modified);

            syncCursorFromVim(win, pos, vPos, mode);
        });
    });
}

void QNVimCore::triggerCommand(const QByteArray &commandId) {
    Core::ActionManager::command(Utils::Id::fromName(commandId))->action()->trigger();
}

void QNVimCore::saveCursorFlashTime(int cursorFlashTime) {
    mSavedCursorFlashTime = cursorFlashTime;

    disconnect(QApplication::styleHints(), &QStyleHints::cursorFlashTimeChanged,
               this, &QNVimCore::saveCursorFlashTime);
    QApplication::setCursorFlashTime(0);
    connect(QApplication::styleHints(), &QStyleHints::cursorFlashTimeChanged,
            this, &QNVimCore::saveCursorFlashTime);
}

bool QNVimCore::eventFilter(QObject *object, QEvent *event) {
    if (!mNVim or !mNVim->isReady())
        return false;

    if (qobject_cast<TextEditor::TextEditorWidget *>(object) ||
        qobject_cast<QPlainTextEdit *>(object) ||
        qobject_cast<Utils::PlainTextEdit *>(object)) {
        if (event->type() == QEvent::Resize) {
            // The resized widget belongs to the view whose window it
            // represents; the editor may be closed before the timer fires.
            const QPointer<Core::IEditor> editorGuard = editorForWidget(object);
            QTimer::singleShot(100, this, [=, this]() {
                fixSize(editorGuard ? editorGuard.data() : Core::EditorManager::currentEditor());
            });
            return false;
        }
    }

    // The text area of an editor is its viewport, not the editor widget: the
    // viewport changes size without the widget being resized when the extra
    // area (line numbers, marks, folding markers) gets wider or narrower, or
    // when a scrollbar appears. The editor wraps its text in the viewport, so
    // the window's grid has to follow the viewport as well.
    if (auto *objectWidget = qobject_cast<QWidget *>(object)) {
        if (Core::IEditor *objectEditor = editorForWidget(objectWidget)) {
            auto *objectTextEditor
                = qobject_cast<TextEditor::TextEditorWidget *>(objectEditor->widget());
            if (objectTextEditor and objectTextEditor->viewport() == objectWidget) {
                if (event->type() == QEvent::Resize) {
                    const QPointer<Core::IEditor> editorGuard = objectEditor;
                    QTimer::singleShot(100, this, [=, this]() {
                        if (editorGuard)
                            fixSize(editorGuard.data());
                    });
                    return false;
                }
            }
        }
    }

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        QString key = NeovimQt::Input::convertKey(*keyEvent);
        mNVim->api2()->nvim_input(key.toUtf8());
        return true;
    } else if (event->type() == QEvent::ShortcutOverride) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        QString key = NeovimQt::Input::convertKey(*keyEvent);
        if (keyEvent->key() == Qt::Key_Escape) {
            mNVim->api2()->nvim_input(key.toUtf8());
        } else {
            keyEvent->accept();
        }
        return true;
    }
    return false;
}

void QNVimCore::editorOpened(Core::IEditor *editor) {
    if (!mEnabled)
        return;

    if (!editor)
        return;

    QString filename(this->filename(editor));
    qDebug(Main) << "Opened " << filename << mSettingBufferFromVim;

    QWidget *widget = editor->widget();
    if (!widget)
        return;

    auto project = ProjectExplorer::ProjectManager::projectForFile(
        Utils::FilePath::fromString(filename));
    qDebug(Main) << project;
    if (project and mNVim and mNVim->isReady()) {
        QString projectDirectory = project->projectDirectory().toFSPathString();
        if (!projectDirectory.isEmpty())
            mNVim->api2()->nvim_command(QStringLiteral("cd %1").arg(projectDirectory).toUtf8());
    }

    if (!qobject_cast<TextEditor::TextEditorWidget *>(widget)) {
        mNumbersColumn->setEditor(nullptr);
        return;
    }
    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());

    // The buffer whose content has to be pushed from the editor once the
    // editor is in its window.
    int bufferToInitialize = 0;

    if (mBuffers.contains(editor)) {
        if (!mSettingBufferFromVim and !mInhibitWindowSync and mNVim and mNVim->isReady()) {
            // The editor became current in Qt Creator: its window shows its
            // buffer and becomes Neovim's current window (a view without a
            // window is given one).
            if (const int win = windowForEditor(editor))
                syncWindow(win, editor);
            else
                ensureWindowForView(editor);
        }
    } else if (const int sharedBuffer = bufferForEditor(editor)) {
        // Another editor shows the same document (a Qt Creator split shows its
        // own editor for the document): they share the Neovim buffer.
        mBuffers[editor] = sharedBuffer;
        setupEditor(editor);
    } else if (mSettingBufferFromVim > 0 and mNVim and mNVim->isReady()) {
        // The editor is created from a Neovim autocommand for a known buffer.
        mBuffers[editor] = mSettingBufferFromVim;
        mEditors[mSettingBufferFromVim] = editor;
        mPendingPush.insert(mSettingBufferFromVim);
        setupEditor(editor);
        bufferToInitialize = mSettingBufferFromVim;
    } else if (mNVim and mNVim->isReady()) {
        setupEditor(editor);
        // Escaping is needed for file names that contain characters
        // which are special in Ex commands (spaces, quotes, ...).
        // Quoting the name is not an option: a `"` starts a comment in
        // Ex commands. fnameescape() does the escaping, so ask Neovim
        // for it.
        auto request = mNVim->api2()->nvim_call_function(QByteArrayLiteral("fnameescape"),
                                                         {filename});
        connect(request, &NeovimQt::MsgpackRequest::finished, this,
                [=, this](quint32, quint64, const QVariant &escaped) {
            auto request = mNVim->api2()->nvim_command(
                QStringLiteral("e %1").arg(escaped.toString()).toUtf8());
            connect(request, &NeovimQt::MsgpackRequest::error, this,
                    [](quint32, quint64, const QVariant &err) {
                        qWarning(Main) << "Could not open file in Neovim:" << err;
                    });
            connect(request, &NeovimQt::MsgpackRequest::finished, this, [=, this]() {
                // Ask for the buffer of the file instead of the current buffer:
                // several editors may be opened at the same time, and another
                // file may be the current one by the time this answer arrives
                // (bufnr('') would return the buffer of that other file).
                auto request = mNVim->api2()->nvim_call_function(
                    QByteArrayLiteral("bufnr"), {filename});
                connect(request, &NeovimQt::MsgpackRequest::finished, this,
                        [=, this](quint32, quint64, const QVariant &v) {
                    const int buffer = v.toInt();
                    if (buffer <= 0) {
                        qWarning(Main) << "Could not find the buffer of" << filename;
                        return;
                    }
                    mBuffers[editor] = buffer;
                    mEditors[buffer] = editor;
                    mPendingPush.insert(buffer);
                    initializeBuffer(buffer);
                    attachEditorToWindow(editor);
                });
            });
        });
    }
    mSettingBufferFromVim = 0;

    mNumbersColumn->setEditor(textEditor);

    widget->setAttribute(Qt::WA_KeyCompression, false);
    widget->installEventFilter(this);
    // The text area of the editor can change size without the editor widget
    // being resized (see eventFilter(): the extra area, a scrollbar), so the
    // viewport has to be watched as well.
    if (QWidget *viewport = textEditor->viewport())
        viewport->installEventFilter(this);

    attachEditorToWindow(editor);

    if (bufferToInitialize)
        initializeBuffer(bufferToInitialize);

    // The editor may be closed again before the timer fires (e.g. an editor a
    // split was created for is replaced right away), so only run for an editor
    // that is still there.
    const QPointer<Core::IEditor> editorGuard(editor);
    QTimer::singleShot(100, this, [this, editorGuard]() {
        if (editorGuard)
            fixSize(editorGuard);
    });
}

void QNVimCore::editorAboutToClose(Core::IEditor *editor) {
    qDebug(Main) << "QNVimPlugin::editorAboutToClose";
    if (!mBuffers.contains(editor))
        return;

    if (Core::EditorManager::currentEditor() == editor)
        mNumbersColumn->setEditor(nullptr);

    const int bufferNumber = mBuffers[editor];
    // Whether Neovim is behind the close (it is deleting the buffer, or closed
    // the window the editor is shown in): the window must then be left alone.
    const bool vimInitiated = mVimInitiatedClose;
    const int win = windowForEditor(editor);
    const QPointer<QWidget> view = editorViewWidget(editor);

    mBuffers.remove(editor);

    // Another editor may still show the same Neovim buffer (e.g. the other half
    // of a Qt Creator split, which shares the buffer); keep the buffer and let
    // one of the remaining editors take over the mapping.
    const QList<Core::IEditor *> remainingEditors = editorsForBuffer(bufferNumber);
    if (!remainingEditors.isEmpty()) {
        Core::IEditor *currentEditor = Core::EditorManager::currentEditor();
        mEditors[bufferNumber] = remainingEditors.contains(currentEditor)
                                     ? currentEditor
                                     : remainingEditors.first();
    } else {
        // When Neovim itself is deleting the buffer, it is already gone or being
        // deleted there; `bd!` would only fail with E516 and show a bogus message.
        if (!mVimInitiatedClose and mNVim and mNVim->isReady())
            mNVim->api2()->nvim_command(QStringLiteral("bd! %1").arg(bufferNumber).toUtf8());

        mEditors.remove(bufferNumber);
        mChangedTicks.remove(bufferNumber);
        mBufferType.remove(bufferNumber);
        mPendingPush.remove(bufferNumber);
    }

    if (win and !vimInitiated and view) {
        // The editor is gone from the view; the window has nothing left to
        // represent if the view stays empty. Wait for the close to be fully
        // processed: another editor may end up in the view (Qt Creator keeps
        // the split with its remaining editors, or picks a replacement).
        QTimer::singleShot(0, this, [this, win, view]() {
            auto it = mWindows.find(win);
            if (it == mWindows.end() or it.value().view != view)
                return; // the window is gone, or another view is bound to it
            if (editorForView(view))
                return; // the view shows another editor now
            if (mWindows.size() <= 1)
                return; // Neovim cannot close its last window
            closeWindowForView(win);
        });
    }
}

QList<Core::IEditor *> QNVimCore::editorsForBuffer(int buffer) const {
    QList<Core::IEditor *> editors;
    for (auto it = mBuffers.cbegin(); it != mBuffers.cend(); ++it) {
        if (it.key() and it.value() == buffer)
            editors.append(it.key());
    }
    return editors;
}

void QNVimCore::closeEditorsForBuffer(int buffer) {
    const QList<Core::IEditor *> editors = editorsForBuffer(buffer);
    if (editors.isEmpty())
        return;

    // The editor activation that follows must not try to switch Neovim to a
    // buffer that is in the middle of being deleted.
    if (editors.contains(Core::EditorManager::currentEditor()))
        mSettingBufferFromVim = -1;

    // Neovim is already deleting the buffer; editors that Qt Creator may open
    // while they are being closed must not `bd!` it again. Closing editors can
    // show a prompt (e.g. to save changes), which runs a nested event loop that
    // can handle another buffer deletion; restoring the previous value keeps
    // this flag armed for the duration of this call.
    const bool vimInitiatedClose = mVimInitiatedClose;
    mVimInitiatedClose = true;
    Core::EditorManager::closeEditors(editors);
    mVimInitiatedClose = vimInitiatedClose;

    mSettingBufferFromVim = 0;
}

void QNVimCore::detectFiletype(int buffer) {
    // Because qnvim registers a BufReadCmd autocommand, Neovim never fires the
    // BufRead event, so its filetype detection never runs for buffers opened
    // through Qt Creator and options set by filetype plugins (such as
    // 'commentstring') are left empty. Detect the filetype manually once the
    // buffer contents have been synced from Qt Creator.
    mNVim->api2()->nvim_command(QStringLiteral("lua if vim.api.nvim_buf_is_valid(%1) then vim.api.nvim_buf_call(%1, function() if vim.bo.filetype == '' then vim.cmd('filetype detect') end end) end").arg(buffer).toUtf8());
}

void QNVimCore::initializeBuffer(int buffer) {
    QString bufferType = mBufferType[buffer];
    if (bufferType == "acwrite" or bufferType.isEmpty()) {
        connect(
            mNVim->api2()->nvim_buf_set_option(buffer, "undolevels", -1),
            &NeovimQt::MsgpackRequest::finished, this, [=, this]() {
                auto editor = mEditors.value(buffer); // the editor may be closed by now
                if (!editor) {
                    mPendingPush.remove(buffer);
                    return;
                }

                // The filename is needed after the (asynchronous) content sync;
                // remember it now, the editor may be gone by then.
                const QString editorFilename = filename(editor);

                syncToVim(editor, [=, this]() {
                    mNVim->api2()->nvim_buf_set_option(buffer, "undolevels", -123456);
                    mNVim->api2()->nvim_buf_set_option(buffer, "modified", false);
                    if (bufferType.isEmpty() && QFile::exists(editorFilename))
                        mNVim->api2()->nvim_buf_set_option(buffer, "buftype", "acwrite");
                    detectFiletype(buffer);
                });
            },
            Qt::DirectConnection);
    } else {
        mNVim->api2()->nvim_buf_set_option(buffer, "modified", false);
        // The content of these buffers (help, terminal) comes from Neovim;
        // there is nothing to push from Qt Creator.
        mPendingPush.remove(buffer);
        syncFromVim();
    }
}

void QNVimCore::handleNotification(const QByteArray &name, const QVariantList &args) {
    if (name == "redraw") {
        redraw(args);
        return;
    }

    if (name == "Gui") {
        QByteArray method = args.first().toByteArray();
        QVariantList methodArgs = args.mid(1);
        if (method == "triggerCommand") {
            for (const auto& methodArg : methodArgs)
                triggerCommand(methodArg.toByteArray());
        } else if (method == "winAutoCommand") {
            const QByteArray event = methodArgs.value(0).toByteArray();
            const int win = methodArgs.value(1).toInt();
            const int buffer = methodArgs.value(2).toInt();
            if (!win)
                return;

            if (event == "WinNew") {
                // A window Neovim created: if it is still nothing but a window
                // once the command that created it was fully processed, it is a
                // split that needs an editor view (reconcileWindows()).
                if (!mWindows.contains(win))
                    mWindows[win].buffer = buffer;
                mNewWindows.insert(win);
                mWindowsDirty = true;
            } else if (event == "WinEnter") {
                if (!mWindows.contains(win))
                    mWindows[win].buffer = buffer;
                mCurrentWindow = win;
                // The user moved to the window in Neovim: show its editor in
                // Qt Creator (the window may not have a view yet).
                if (!mInhibitWindowSync)
                    activateWindowEditor(win);
                // A window that just became visible (e.g. in a tabpage that
                // was entered) can be sized to its view now.
                resizeWindow(win);
            } else if (event == "WinClosed") {
                if (mClosingWindows.remove(win))
                    return; // qnvim closed the window (its editor view is gone)

                mNewWindows.remove(win);
                auto windowState = mWindows.find(win);
                if (windowState == mWindows.end())
                    return;

                const WindowState st = *windowState;
                const bool current = (mCurrentWindow == win);
                // The window has a view that Qt Creator must remove as well; a
                // view the editor was moved out of is left alone.
                const bool viewRemoved = st.view and st.editor
                                         and editorViewWidget(st.editor) == st.view
                                         and windowForView(st.view) == win;
                mWindows.erase(windowState);
                if (current)
                    mCurrentWindow = 0; // the WinEnter of the new current window follows

                if (!viewRemoved)
                    return;

                // The editors in the view are closed as part of the removal;
                // their buffers are kept (Neovim closed a window, not a buffer).
                Core::EditorManager::activateEditor(st.editor);
                const bool removingView = mRemovingView;
                const bool vimInitiatedClose = mVimInitiatedClose;
                const bool inhibited = mInhibitWindowSync;
                mRemovingView = true;
                mVimInitiatedClose = true;
                mInhibitWindowSync = true;
                triggerCommand(QByteArrayLiteral("QtCreator.RemoveCurrentSplit"));
                mInhibitWindowSync = inhibited;
                mRemovingView = removingView;
                mVimInitiatedClose = vimInitiatedClose;
            }
        } else if (method == "fileAutoCommand") {
            QByteArray cmd = methodArgs.first().toByteArray();
            int buffer = methodArgs[1].toByteArray().toInt();
            QString filename = QString::fromUtf8(methodArgs[2].toByteArray());
            QString bufferType = QString::fromUtf8(methodArgs[3].toByteArray());
            bool bufferListed = methodArgs[4].toInt();
            QString bufferHidden = QString::fromUtf8(methodArgs[5].toByteArray());
            bool alwaysText = methodArgs[6].toInt();

            if (cmd == "BufReadCmd" or cmd == "TermOpen") {
                mBufferType[buffer] = bufferType;
                if (mEditors.contains(buffer)) {
                    mText.clear();
                    // The buffer is being (re)read (Neovim does not read it, the
                    // BufReadCmd autocommand suppresses that); its content has to
                    // come from Qt Creator again.
                    mPendingPush.insert(buffer);
                    initializeBuffer(buffer);
                } else {
                    if (cmd == "TermOpen")
                        mNVim->api2()->nvim_command("doautocmd BufEnter");
                }
            } else if (cmd == "BufWriteCmd") {
                Core::IEditor *bufferEditor = mEditors.value(buffer);
                if (bufferEditor) {
                    QString currentFilename = this->filename(bufferEditor);
                    if (bufferEditor->document()->save(Utils::FilePath::fromString(filename))) {
                        if (currentFilename != filename) {
                            mEditors.remove(buffer);
                            mChangedTicks.remove(buffer);
                            mBuffers.remove(bufferEditor);

                            auto request = mNVim->api2()->nvim_buf_set_name(buffer, filename.toUtf8());
                            connect(request, &NeovimQt::MsgpackRequest::finished, this, [=, this](quint32, quint64, const QVariant &) {
                                mNVim->api2()->nvim_command("edit!");
                            });
                        } else {
                            mNVim->api2()->nvim_buf_set_option(buffer, "modified", false);
                        }
                    } else {
                        mNVim->api2()->nvim_buf_set_option(buffer, "modified", true);
                    }
                }
            } else if (cmd == "BufEnter") {
                mBufferType[buffer] = bufferType;
                // The window the buffer was entered in (the preamble reports 0
                // for windows without a counterpart in the editor area).
                const int win = methodArgs.value(7).toInt();
                if (win and mNewWindows.contains(win)) {
                    // The window has no editor view yet: remember what it ended
                    // up showing; its view is created once the command that
                    // created the window was fully processed.
                    mWindows[win].buffer = buffer;
                    mWindowsDirty = true;
                    return;
                }
                if (win and mWindows.value(win).bound) {
                    // The window has an editor view: it shows the buffer now.
                    updateWindowBuffer(win, buffer, filename, bufferType, alwaysText);
                    return;
                }

                // Neovim changed the buffer shown where Qt Creator has no
                // (or not yet a) window: open the buffer's editor.
                Core::IEditor *editor = Core::EditorManager::currentEditor();
                if (!editor or !mBuffers.contains(editor))
                    return;

                [[maybe_unused]] Core::IEditor *e = nullptr;
                mSettingBufferFromVim = buffer;
                if (!filename.isEmpty() and filename != this->filename(editor)) {
                    if (mEditors.contains(buffer)) {
                        if (editor != mEditors[buffer]) {
                            Core::EditorManager::activateEditor(
                                mEditors[buffer]);
                            e = mEditors[buffer];
                        }
                    } else {
                        if (bufferType.isEmpty() && QFile::exists(filename)) {
                            QFileInfo fileInfo = QFileInfo(filename);
                            if (alwaysText) {
                                if ((QStringList() << "js"
                                                   << "qml"
                                                   << "cpp"
                                                   << "c"
                                                   << "cc"
                                                   << "hpp"
                                                   << "h"
                                                   << "pro")
                                        .contains(fileInfo.suffix(), Qt::CaseInsensitive))
                                    e = Core::EditorManager::openEditor(Utils::FilePath::fromString(filename));
                                else
                                    e = Core::EditorManager::openEditor(Utils::FilePath::fromString(filename), "Core.PlainTextEditor");
                            } else {
                                e = Core::EditorManager::openEditor(Utils::FilePath::fromString(filename));
                            }
                        } else {
                            if (bufferType == "terminal") {
                                e = Core::EditorManager::openEditorWithContents("Terminal", &filename, QByteArray(), filename);
                            } else if (bufferType == "help") {
                                e = Core::EditorManager::openEditorWithContents("Help", &filename, QByteArray(), filename);
                                e->document()->setFilePath(Utils::FilePath::fromString(filename));
                                e->document()->setPreferredDisplayName(filename);
                            } else {
                                e = Core::EditorManager::openEditorWithContents("Terminal", &filename, QByteArray(), filename);
                            }
                        }
                    }
                }
                mSettingBufferFromVim = 0;
                // if (filename.isEmpty()) {
                //     // callback();
                // } else {
                //     connect(mNVim->api2()->nvim_command("try | silent only! | catch | endtry"), &NeovimQt::MsgpackRequest::finished, callback);
                // }
            } else if (cmd == "BufDelete") {
                if (bufferListed)
                    closeEditorsForBuffer(buffer);
            } else if (cmd == "BufHidden") {
                if (
                    bufferHidden == "wipe" or bufferHidden == "delete" or bufferHidden == "unload" or bufferType == "help")
                    closeEditorsForBuffer(buffer);
            } else if (cmd == "BufWipeout") {
                if (!bufferListed)
                    closeEditorsForBuffer(buffer);
            }
        }
    }
}

void QNVimCore::redraw(const QVariantList &args) {
    bool shouldSync = false;
    bool flush = false;

    for (const auto& arg : args) {
        QVariantList line = arg.toList();
        if (line.isEmpty())
            continue;

        const QByteArray command = line.first().toByteArray();

        if (!command.startsWith("msg") and
            !command.startsWith("cmdline") and command != "flush")
            shouldSync = true;

        if (command == "flush") {
            flush = true;
            continue;
        }

        // A line holds the args of one or more events of the same kind: Neovim
        // merges consecutive events of one kind into one line (several
        // win_float_pos events, the grid lines of a redraw, ...).
        for (int event = 1; event < line.size(); ++event)
            redrawEvent(command, line[event].toList());
    }

    if (flush) {
        // Neovim has fully processed the command that caused this redraw: the
        // windows and the editor views can be reconciled.
        closeWindowsForRemovedViews();
        reconcileWindows();

        // The float overlays may have to move: they are placed relative to the
        // text area of the current editor, which may have changed.
        placeFloats();
    }

    if (shouldSync and flush)
        syncFromVim();

    updateCursorSize();
}

void QNVimCore::redrawEvent(const QByteArray &command, const QVariantList &args) {
    if (command == "win_pos") {
        // ["win_pos", grid, win, start_row, start_col, width, height]: the
        // window's grid, its position in Neovim's layout and its size. The grid
        // is what gets resized to the editor view (see resizeWindow()); the
        // position places the floats the window anchors.
        if (args.size() >= 4) {
            const int grid = args[0].toInt();
            const int win = args[1].toInt();
            if (grid and win) {
                // A grid remembered before it turned out to be a window's (its
                // content may have arrived first) is not a float.
                if (auto state = mFloats.find(grid);
                    state != mFloats.end() and !state->isFloat)
                    closeFloat(grid);

                WindowState &st = mWindows[win];
                st.startRow = args[2].toInt();
                st.startCol = args[3].toInt();
                if (st.grid != grid) {
                    st.grid = grid;
                    st.gridWidth = st.gridHeight = 0;
                    resizeWindow(win);
                }
            }
        }
    } else if (command == "win_viewport") {
        // ["win_viewport", grid, win, ...]: also tells which grid the window is
        // drawn in, for windows whose win_pos is old (e.g. a window qnvim did
        // not see when its layout was last reported).
        if (args.size() >= 2) {
            const int grid = args[0].toInt();
            const int win = args[1].toInt();
            if (auto state = mFloats.find(grid);
                state != mFloats.end() and !state->isFloat)
                closeFloat(grid);

            auto windowState = mWindows.find(win);
            if (grid and windowState != mWindows.end() and windowState.value().grid != grid) {
                windowState.value().grid = grid;
                windowState.value().gridWidth = windowState.value().gridHeight = 0;
                resizeWindow(win);
            }
        }
    } else if (command == "win_float_pos") {
        // ["win_float_pos", grid, win, anchor, anchor_grid, anchor_row,
        //  anchor_col, mouse_enabled, zindex, compindex, screen_row,
        //  screen_col]: Neovim shows a floating window, or reconfigures it. It
        // is shown as an overlay at screen_row/screen_col, which are cells of
        // Neovim's screen (grid 1). Neovim versions that do not report the
        // screen position (older than the compindex/position fields) leave
        // their floats invisible, as they always were.
        if (args.size() >= 11) {
            const int grid = args[0].toInt();
            const int win = args[1].toInt();
            if (grid and win) {
                FloatState &st = mFloats[grid];
                st.isFloat = true;
                st.win = win;
                st.anchorGrid = args[3].toInt();
                st.visible = true;
                st.mouse = args[6].toBool();
                st.compindex = args[8].toInt();
                st.row = args[9].toInt();
                st.col = args[10].toInt();
            }
        }
    } else if (command == "win_hide") {
        // ["win_hide", grid]: the window is not shown right now; it may come
        // back (e.g. when its tabpage is entered again).
        if (!args.isEmpty()) {
            auto floatState = mFloats.find(args[0].toInt());
            if (floatState != mFloats.end() and floatState->isFloat) {
                floatState->visible = false;
                if (floatState->widget)
                    floatState->widget->hide();
            }
        }
    } else if (command == "win_close") {
        // ["win_close", grid]: the float is closed (its grid follows with
        // grid_destroy).
        if (!args.isEmpty())
            closeFloat(args[0].toInt());
    } else if (command == "grid_resize") {
        // ["grid_resize", grid, width, height]
        if (args.size() >= 3) {
            const int grid = args[0].toInt();
            const int width = args[1].toInt();
            const int height = args[2].toInt();
            if (grid == 1) {
                mWidth = width;
                mHeight = height;
            } else {
                rememberGrid(grid);
                setFloatGridSize(grid, height, width);
            }
        }
    } else if (command == "grid_line") {
        // ["grid_line", grid, row, col_start, cells, wrap]: the cells of a
        // float's grid (the editors draw their own documents).
        if (args.size() >= 4) {
            const int grid = args[0].toInt();
            if (!isWindowGrid(grid)) {
                rememberGrid(grid);
                setFloatGridLine(grid, args[1].toInt(), args[2].toInt(), args[3].toList());
            }
        }
    } else if (command == "grid_scroll") {
        // ["grid_scroll", grid, top, bot, left, right, rows, cols]
        if (args.size() >= 7) {
            const int grid = args[0].toInt();
            if (!isWindowGrid(grid)) {
                rememberGrid(grid);
                scrollFloatGrid(grid, args[1].toInt(), args[2].toInt(), args[3].toInt(),
                                args[4].toInt(), args[5].toInt(), args[6].toInt());
            }
        }
    } else if (command == "grid_clear") {
        // ["grid_clear", grid]
        if (!args.isEmpty()) {
            const int grid = args[0].toInt();
            if (!isWindowGrid(grid)) {
                rememberGrid(grid);
                clearFloatGrid(grid);
            }
        }
    } else if (command == "grid_destroy") {
        // ["grid_destroy", grid]: the grid will not be used any more.
        if (!args.isEmpty()) {
            const int grid = args[0].toInt();
            mGridSizes.remove(grid);
            closeFloat(grid);
        }
    } else if (command == "hl_attr_define") {
        // ["hl_attr_define", id, rgb_attr, cterm_attr, info]: the colors and
        // styles the floats are drawn with (the editors use Qt Creator's
        // colors). The rgb attributes are resolved colors, or -1 for a color
        // Neovim leaves to the UI.
        if (args.size() >= 2) {
            const QVariantMap attributes = args[1].toMap();
            HlAttr &hl = mHlAttrs[args[0].toInt()];
            const int foreground = attributes.value("foreground", -1).toInt();
            const int background = attributes.value("background", -1).toInt();
            hl.foreground = foreground < 0 ? QColor() : QColor::fromRgb(QRgb(foreground));
            hl.background = background < 0 ? QColor() : QColor::fromRgb(QRgb(background));
            hl.blend = attributes.value("blend", 0).toInt();
            hl.reverse = attributes.value("reverse", false).toBool();
            hl.bold = attributes.value("bold", false).toBool();
            hl.italic = attributes.value("italic", false).toBool();
            hl.underline = attributes.value("underline", false).toBool();
        }
    } else if (command == "bell") {
        QApplication::beep();
    } else if (command == "mode_change") {
        mUIMode = args.first().toByteArray();
    } else if (command == "busy_start") {
        mBusy = true;
    } else if (command == "busy_stop") {
        mBusy = false;
    } else if (command == "mouse_on") {
        mMouse = true;
    } else if (command == "mouse_off") {
        mMouse = false;
    } else if (command == "default_colors_set") {
        auto editor = Core::EditorManager::currentEditor();
        auto textEditor = editor ? qobject_cast<TextEditor::TextEditorWidget *>(editor->widget())
                                 : nullptr;

        qint64 val = args[0].toLongLong();
        if (val != -1) {
            mForegroundColor = QRgb(val);
            if (textEditor) {
                QPalette palette = textEditor->palette();
                palette.setColor(QPalette::WindowText, mForegroundColor);
                textEditor->setPalette(palette);
            }
        }

        val = args[1].toLongLong();
        if (val != -1) {
            mBackgroundColor = QRgb(val);
            if (textEditor) {
                QPalette palette = textEditor->palette();
                palette.setBrush(QPalette::Window, mBackgroundColor);
                textEditor->setPalette(palette);
            }
        }

        val = args[2].toLongLong();
        if (val != -1) {
            mSpecialColor = QRgb(val);
        }
    } else if (command == "cmdline_show") {
        auto content = QByteArray();
        for (const auto &contentItem : args[0].toList())
            content += contentItem.toList()[1].toByteArray();

        auto pos = args[1].toInt();
        auto firstc = args[2].toString()[0];
        auto prompt = QString::fromUtf8(args[3].toByteArray());
        auto indent = args[4].toInt();

        m_cmdLine->onCmdLineShow(QString::fromUtf8(content), pos, firstc, prompt, indent);
    } else if (command == "cmdline_pos") {
        m_cmdLine->onCmdLinePos(args[0].toInt());
    } else if (command == "cmdline_hide") {
        m_cmdLine->onCmdLineHide();
    } else if (command == "msg_show") {
        auto message = QString();
        for (const auto &contentItem : args[1].toList())
            message += QString::fromUtf8(contentItem.toList()[1].toByteArray());

        m_cmdLine->showMessage(message);
    } else if (command == "msg_clear") {
        m_cmdLine->clear();
    } else if (command == "msg_history_show") {
        auto message = QString();
        for (const auto &entry : args[1].toList()) {
            for (const auto &contentItem : entry.toList()[1].toList())
                message += QString::fromUtf8(contentItem.toList()[1].toByteArray()) + '\n';
        }

        m_cmdLine->showMessage(message);
    }
}

void QNVimCore::updateCursorSize() {
    auto editor = Core::EditorManager::currentEditor();
    if (!editor)
        return;

    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    if (!textEditor)
        return;

    QFontMetricsF textEditorFontMetric(textEditor->textDocument()->fontSettings().font());

    if (mBusy) {
        textEditor->setCursorWidth(0);
    } else if (mUIMode == "insert" or mUIMode == "visual") {
        textEditor->setCursorWidth(1);
    } else if (mUIMode == "normal" or mUIMode == "operator") {
        textEditor->setCursorWidth(static_cast<int>(textEditorFontMetric.horizontalAdvance('A') * textEditor->textDocument()->fontSettings().fontZoom() / 100));
    }

    mNumbersColumn->updateGeometry();
}

// Neovim's floating windows (a picker, a hover popup, ...) are mirrored as
// overlays inside Qt Creator's main window. Neovim sends the float's position
// on its screen (win_float_pos: screen_row/screen_col, cells of grid 1) and the
// cells of its grid (grid_line, grid_scroll, grid_clear, grid_resize), and the
// overlay draws them with the editor's font. Neovim's screen is the text area
// of the current editor (grid 1 is sized to it, see fixSize()), so that editor
// gives both the font and the position of the cells on the screen.
FloatCell QNVimCore::floatCell(int hlId, const QString &text) const {
    auto attribute = mHlAttrs.constFind(hlId);
    const HlAttr hl = attribute == mHlAttrs.constEnd() ? HlAttr() : attribute.value();

    QColor foreground = hl.foreground.isValid() ? hl.foreground : mForegroundColor;
    QColor background = hl.background.isValid() ? hl.background : mBackgroundColor;
    if (hl.reverse)
        qSwap(foreground, background);
    if (hl.blend > 0)
        background.setAlpha(255 * (100 - hl.blend) / 100);

    FloatCell cell;
    cell.text = text;
    cell.foreground = foreground;
    cell.background = background;
    cell.bold = hl.bold;
    cell.italic = hl.italic;
    cell.underline = hl.underline;
    return cell;
}

// A grid Neovim draws in: qnvim remembers its content as soon as an event for it
// arrives, because the event that says what the grid is (win_float_pos for a
// float, win_pos for a window) can come later. A grid that turns out to be a
// window's is dropped again (see redrawEvent()); the screen (grid 1) is not a
// grid qnvim draws.
void QNVimCore::rememberGrid(int grid) {
    if (grid == 1 or mFloats.contains(grid) or isWindowGrid(grid))
        return;

    mFloats.insert(grid, FloatState());
}

bool QNVimCore::isWindowGrid(int grid) const {
    for (const auto &windowState : mWindows) {
        if (windowState.grid == grid)
            return true;
    }

    return false;
}

FloatWidget *QNVimCore::floatWidget(int grid) {
    auto floatState = mFloats.find(grid);
    if (floatState == mFloats.end())
        return nullptr;

    if (!floatState->widget) {
        auto mainWindow = Core::ICore::mainWindow();
        if (!mainWindow)
            return nullptr;

        floatState->widget = new FloatWidget(mainWindow);
        if (const QSize size = mGridSizes.value(grid); size.isValid())
            floatState->widget->resizeGrid(size.height(), size.width());
        floatState->widget->hide(); // placeFloats() shows it
    }

    return floatState->widget;
}

void QNVimCore::setFloatGridSize(int grid, int rows, int cols) {
    mGridSizes.insert(grid, QSize(cols, rows));

    if (mFloats.contains(grid))
        if (FloatWidget *widget = floatWidget(grid))
            widget->resizeGrid(rows, cols);
}

void QNVimCore::setFloatGridLine(int grid, int row, int col, const QVariantList &cells) {
    FloatWidget *widget = floatWidget(grid);
    if (!widget)
        return;

    QVector<FloatCell> line;
    int hlId = 0;
    for (const QVariant &cellValue : cells) {
        const QVariantList cell = cellValue.toList();
        if (cell.isEmpty())
            continue;

        const QString text = QString::fromUtf8(cell[0].toByteArray());
        if (cell.size() >= 2)
            hlId = cell[1].toInt();
        const int repeat = cell.size() >= 3 ? cell[2].toInt() : 1;

        // A cell can hold more than one character, and the right half of a
        // double width character is a cell with an empty text.
        for (int r = 0; r < repeat; ++r) {
            for (int i = 0; i < text.size(); ++i) {
                QString character = text.at(i);
                if (character.at(0).isHighSurrogate() and i + 1 < text.size())
                    character += text.at(++i);
                line.append(floatCell(hlId, character));
            }
        }
    }

    widget->setLine(row, col, line);
}

void QNVimCore::scrollFloatGrid(int grid, int top, int bot, int left, int right, int rows,
                                int cols) {
    if (FloatWidget *widget = floatWidget(grid))
        widget->scrollGrid(top, bot, left, right, rows, cols);
}

void QNVimCore::clearFloatGrid(int grid) {
    if (FloatWidget *widget = floatWidget(grid))
        widget->clearGrid();
}

void QNVimCore::closeFloat(int grid) {
    auto floatState = mFloats.find(grid);
    if (floatState == mFloats.end())
        return;

    delete floatState->widget.data();
    mFloats.erase(floatState);
}

void QNVimCore::clearFloats() {
    for (auto &floatState : mFloats) {
        delete floatState.widget.data();
        floatState.widget = nullptr;
    }
    mFloats.clear();
}

// Where the editor draws the first character of its first visible row, in
// global coordinates. The extra area (line numbers, marks, folding markers) is
// outside the viewport, so the cursor rects are the way to ask.
static QPoint editorTextOrigin(const TextEditor::TextEditorWidget *textEditor) {
    const QTextCursor firstVisibleCursor = textEditor->cursorForPosition(QPoint(0, 0));
    QTextCursor firstColumnCursor = firstVisibleCursor;
    firstColumnCursor.movePosition(QTextCursor::StartOfBlock);
    const QPoint viewport = textEditor->viewport()->mapToGlobal(QPoint(0, 0));
    return viewport + QPoint(textEditor->cursorRect(firstColumnCursor).x(),
                             textEditor->cursorRect(firstVisibleCursor).y());
}

void QNVimCore::placeFloats() {
    if (mFloats.isEmpty())
        return;

    auto mainWindow = Core::ICore::mainWindow();
    auto editor = Core::EditorManager::currentEditor();
    auto textEditor = editor ? qobject_cast<TextEditor::TextEditorWidget *>(editor->widget())
                             : nullptr;
    if (!mainWindow or !textEditor) {
        for (auto &floatState : mFloats) {
            if (floatState.widget)
                floatState.widget->hide();
        }
        return;
    }

    const QFont font = textEditor->textDocument()->fontSettings().font();
    const QFontMetricsF metrics(font);
    const qreal cellWidth = metrics.horizontalAdvance(QLatin1Char('A'));
    const qreal cellHeight = metrics.lineSpacing();
    if (cellWidth <= 0 or cellHeight <= 0)
        return;

    const QPoint windowOrigin = mainWindow->mapToGlobal(QPoint(0, 0));

    // The bottom of the current editor's text area: the editor view puts its
    // cmdline widget right below the text, which is where Neovim's screen has
    // the cmdline (right below the window).
    const QPoint editorTextBottom
        = textEditor->viewport()->mapToGlobal(QPoint(0, textEditor->viewport()->height()));
    const qreal textBottom = editorTextBottom.y() - windowOrigin.y();

    // The cursor's row on Neovim's screen, the row the current window's text
    // ends at, and the grid of the window itself: a screen-anchored float that
    // sits right at one of those rows was placed at a cell of the editor's
    // layout (see below).
    int cursorRow = 0;
    int textEndRow = 0;
    int cursorGrid = 0;
    if (const auto current = mWindows.constFind(mCurrentWindow);
        current != mWindows.constEnd() and current.value().editor) {
        auto *cursorEditor = qobject_cast<TextEditor::TextEditorWidget *>(
            current.value().editor->widget());
        if (cursorEditor) {
            const QTextCursor firstVisible = cursorEditor->cursorForPosition(QPoint(0, 0));
            const int textRow = qRound(
                (cursorEditor->cursorRect().top() - cursorEditor->cursorRect(firstVisible).top())
                / cellHeight);
            cursorRow = current.value().startRow + current.value().winbar + textRow;
            textEndRow = current.value().startRow + current.value().gridHeight;
            cursorGrid = current.value().grid;
        }
    }

    // Where a float is placed: relative to the cell (0, 0) of the grid Neovim
    // anchored it to (win_float_pos' anchor_grid). The screen (grid 1) is the
    // whole main window; a window's grid is the text area of its editor view
    // (minus its external columns and winbar, which are part of the grid but
    // which the editor does not show); another float's grid is that float's own
    // position.
    struct GridOrigin {
        QPointF origin; // in the main window's coordinates
        int row = 0;    // the screen cell that origin is at
        int col = 0;
    };

    QHash<int, GridOrigin> origins; // of the grids the floats reported
    const std::function<GridOrigin(int, int)> originOf = [&](int grid, int depth) -> GridOrigin {
        if (depth > 8) // a cycle, or nesting too deep: fall back to the screen
            return GridOrigin();

        if (const auto known = origins.constFind(grid); known != origins.constEnd())
            return known.value();

        if (grid != 1) {
            if (auto floatState = mFloats.constFind(grid);
                floatState != mFloats.constEnd() and floatState->isFloat) {
                const GridOrigin anchor = originOf(floatState->anchorGrid, depth + 1);
                GridOrigin origin;
                origin.origin = anchor.origin
                                + QPointF((floatState->col - anchor.col) * cellWidth,
                                          (floatState->row - anchor.row) * cellHeight);
                origin.row = floatState->row;
                origin.col = floatState->col;
                origins.insert(grid, origin);
                return origin;
            }

            // A window's grid: the window is shown in an editor view.
            for (const WindowState &windowState : mWindows) {
                if (windowState.grid != grid or !windowState.editor or !windowState.view)
                    continue;
                auto *windowTextEditor
                    = qobject_cast<TextEditor::TextEditorWidget *>(windowState.editor->widget());
                if (windowTextEditor) {
                    GridOrigin origin;
                    origin.origin = editorTextOrigin(windowTextEditor) - windowOrigin
                                    - QPointF(windowState.textoff * cellWidth,
                                              windowState.winbar * cellHeight);
                    origin.row = windowState.startRow;
                    origin.col = windowState.startCol;
                    origins.insert(grid, origin);
                    return origin;
                }
                break;
            }
        }

        return GridOrigin(); // the screen: the window's top left corner
    };

    // Neovim's compindex is the order the floats are stacked in: place them in
    // it, so that raising each one leaves the last (topmost) one on top.
    QList<int> grids = mFloats.keys();
    std::sort(grids.begin(), grids.end(), [this](int first, int second) {
        return mFloats.value(first).compindex < mFloats.value(second).compindex;
    });

    // The rectangles (in screen cells) of the floats classified as belonging to
    // the editor in this pass, so that a float lying inside one of them (a
    // plugin's scrollbar inside its menu) is classified the same way.
    QList<QPair<QRect, bool>> editorFloats;

    for (const int grid : grids) {
        auto floatState = mFloats.find(grid);
        if (floatState == mFloats.end() or !floatState->isFloat)
            continue;

        FloatWidget *widget = floatWidget(grid);
        if (!widget)
            continue;

        widget->setGridFont(font);
        widget->setDefaultColors(mForegroundColor, mBackgroundColor);

        if (!floatState->visible or widget->rows() <= 0 or widget->cols() <= 0) {
            widget->hide();
            continue;
        }

        // A float Neovim anchors to the screen is positioned in Neovim's screen
        // cells, which qnvim maps onto the whole main window. A plugin that
        // wants a popup at the cursor or at the cmdline, though, computes a
        // cell of the editor's layout (screenpos(), screenline(), ...) and
        // opens an editor-anchored float there: that one belongs to the editor
        // view, like the floats Neovim anchors to a window itself. Tall floats
        // are pickers and other panes: they are positioned against the window,
        // not the cursor.
        if (floatState->anchorGrid == 1) {
            if (floatState->classifiedRow != floatState->row
                or floatState->classifiedCol != floatState->col) {
                floatState->classifiedRow = floatState->row;
                floatState->classifiedCol = floatState->col;
                floatState->cursorGrid = 0;
                floatState->atCmdline = false;
                const bool small = mHeight <= 0 or widget->rows() * 3 <= mHeight * 2;
                const bool atCursorRow = qAbs(floatState->row - cursorRow) <= 1
                                         or qAbs(floatState->row + widget->rows() - cursorRow)
                                                <= 1;
                const bool belowText
                    = floatState->row + widget->rows() > textEndRow; // the cmdline
                if (cursorGrid and small and (atCursorRow or belowText)) {
                    floatState->cursorGrid = cursorGrid;
                    floatState->atCmdline = belowText;
                }
            }

            // A float that lies inside one already classified (a plugin's
            // scrollbar inside its menu) belongs with it.
            const QRect floatRect(floatState->col, floatState->row, widget->cols(),
                                  widget->rows());
            if (!floatState->cursorGrid) {
                for (const auto &[rect, cmdline] : editorFloats) {
                    if (rect.contains(floatRect)) {
                        floatState->cursorGrid = cursorGrid;
                        floatState->atCmdline = cmdline;
                        break;
                    }
                }
            }
            if (floatState->cursorGrid)
                editorFloats.append({floatRect, floatState->atCmdline});
        } else {
            floatState->cursorGrid = 0;
            floatState->atCmdline = false;
        }

        const int anchorGrid = floatState->anchorGrid == 1 and floatState->cursorGrid
                                   ? floatState->cursorGrid
                                   : floatState->anchorGrid;
        const GridOrigin anchor = originOf(anchorGrid, 0);
        QPointF origin = anchor.origin
                         + QPointF((floatState->col - anchor.col) * cellWidth,
                                   (floatState->row - anchor.row) * cellHeight);

        if (floatState->atCmdline) {
            // Neovim's screen has the cmdline right below the window's text,
            // the editor view has its cmdline widget there. Keep the float
            // above it, so that a completion menu for a `:` command does not
            // cover the command being typed.
            origin.setY(qMin(origin.y(), textBottom - widget->rows() * cellHeight));
        }

        widget->setGeometry(QRect(origin.toPoint(), widget->gridSize()));
        widget->show();
        widget->raise();
    }
}

} // namespace Internal
} // namespace QNVim
