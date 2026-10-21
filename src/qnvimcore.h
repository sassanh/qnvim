#pragma once

#include "numbers_column.h"
#include "qnvimfloat.h"

#include <QColor>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QSet>
#include <QSize>

namespace Core {
class IEditor;
}

namespace ProjectExplorer {
class Project;
}

namespace NeovimQt {
class NeovimConnector;
}

namespace QNVim {
namespace Internal {

class CmdLine;

/**
 * Encapsulates plugin's behavior with an assumption, that it is enabled.
 */
class QNVimCore : public QObject {
    Q_OBJECT
  public:
    explicit QNVimCore(QObject *parent = nullptr);
    virtual ~QNVimCore();

    NeovimQt::NeovimConnector *nvimConnector() const { return mNVim; }

    bool eventFilter(QObject *object, QEvent *event) override;

  protected:
    QString filename(Core::IEditor * = nullptr) const;

    void fixSize(Core::IEditor * = nullptr);
    // Neovim -> Qt Creator: make the window's grid exactly as big as the text
    // area of the editor view that represents the window, so that Neovim wraps
    // lines where the editor wraps them and scrolls by the same amount of
    // lines. The width every window's grid needs for its external columns is
    // read from Neovim first (see refreshWindowSizes()).
    void resizeWindow(int win);
    // Read every window's external columns from Neovim, then size the grid of
    // every bound window (see resizeWindow()).
    void refreshWindowSizes();
    void syncCursorToVim(Core::IEditor * = nullptr);
    void syncSelectionToVim(Core::IEditor * = nullptr);
    void syncModifiedToVim(Core::IEditor * = nullptr);
    void syncToVim(Core::IEditor * = nullptr, std::function<void()> = nullptr);
    void syncCursorFromVim(int win, const QVariantList &, const QVariantList &, QByteArray mode);
    void syncFromVim();

    void triggerCommand(const QByteArray &);

  private slots:
    // Save cursor flash time to variable instead of changing real value
    void saveCursorFlashTime(int cursorFlashTime);

  private:
    // Neovim is ready: set it up. Neovim notifies qnvim through the events of
    // the "Gui" autocommands (defined by src/preamble.vim) and through buffer
    // updates; the UI attachment makes Neovim draw at all.
    void setupNeovim();
    // Qt Creator -> Neovim: send the VimL setup (src/preamble.vim) as one
    // `:command`, prefixed by qnvim's channel number (g:neovim_channel).
    void sendPreamble();
    // Attach qnvim as Neovim's UI: the ext_* options decide which events
    // Neovim sends (see the redraw notifications), and the editor the user is
    // in is opened once the attachment is done.
    void attachUi();
    // Neovim -> Qt Creator: subscribe to the notifications qnvim handles (see
    // handleNotification()).
    void installGuiHooks();

    void editorOpened(Core::IEditor *);
    void editorAboutToClose(Core::IEditor *);

    QList<Core::IEditor *> editorsForBuffer(int) const;
    void closeEditorsForBuffer(int);

    void initializeBuffer(int);
    void detectFiletype(int);
    void handleNotification(const QByteArray &, const QVariantList &);
    void redraw(const QVariantList &);
    // One event of a redraw notification ("win_pos", "grid_line", ...), with
    // the args of that one event.
    void redrawEvent(const QByteArray &, const QVariantList &);
    void updateCursorSize();
    // The column is a child of the editor it is shown in and can be destroyed
    // with it; recreate it if that happened.
    NumbersColumn *numbersColumn();

    // A Neovim window and the editor view that represents it. A window shows
    // the buffer of the editor that is current in its view; the window and the
    // editor view are each other's counterparts (Neovim's `:split` and
    // `:vsplit` show up as Qt Creator splits, and the other way around). A
    // window may be known (e.g. from WinNew or BufEnter) before a view is bound
    // to it (see `bound` below).
    struct WindowState {
        // The editor current in the view (null, e.g. while the view is empty).
        QPointer<Core::IEditor> editor;
        // The editor view widget (the editor's grandparent widget, see
        // editorViewWidget()).
        QPointer<QWidget> view;
        // Whether a view was ever bound to the window. A window without a view
        // either never had one, or its view was destroyed in Qt Creator (a null
        // view with `bound` set).
        bool bound = false;
        // The buffer shown in the window, as far as qnvim knows.
        int buffer = 0;
        // Cursor position last synced with this window: 1-based line and
        // column (0 while nothing was synced).
        int cursorLine = 0;
        int cursorCol = 0;
        int vCursorLine = 0;
        int vCursorCol = 0;
        // Top line of the window's viewport as last seen, and the top line to
        // be applied to the editor (0 while there is none).
        int topline = 0;
        int pendingTopline = 0;
        // The grid Neovim draws the window in (0 while unknown; see the
        // win_pos/win_viewport events). Only a grid known to qnvim can be
        // sized to the editor view the window is represented by.
        int grid = 0;
        // The width of the window's external columns (line numbers, sign and
        // fold columns, 'statuscolumn'). They are part of the grid, not of the
        // text: for the text to fill the view, the grid must be that many
        // columns wider than the view (-1 while unknown).
        int textoff = -1;
        // The rows of the winbar (dropbar sets one), which is grid row 0 of the
        // window: the window's text starts that many rows lower.
        int winbar = 0;
        // Where the window sits on Neovim's screen, in cells (see win_pos).
        // Its grid starts there; the editor view an editor is drawn in can be
        // anywhere, so floats anchored to the window are placed relative to
        // their editor view instead.
        int startRow = 0;
        int startCol = 0;
        // The size last requested for the grid, to not request it again.
        int gridWidth = 0;
        int gridHeight = 0;
    };

    // A floating window Neovim shows (a picker, a hover popup, ...): where it is
    // on Neovim's screen, how it stacks with the other floats and the overlay
    // that shows it (see qnvimfloat.h). A grid whose events qnvim saw is
    // remembered with this state as well, before it is known to be a float
    // (win_float_pos) or a window's grid (win_pos): `isFloat` says which it
    // turned out to be.
    struct FloatState {
        bool isFloat = false;
        int win = 0;
        // The grid the float is anchored to (win_float_pos' anchor_grid): the
        // screen (grid 1, the whole main window), a window's grid, or another
        // float's grid. The float is placed relative to the cell (0, 0) of that
        // grid, which is not the same as the top left of Neovim's screen for a
        // window's grid: the window is shown in an editor view somewhere.
        int anchorGrid = 1;
        // The window's grid a screen-anchored float is placed relative to
        // instead of the screen (0: none, see placeFloats()): plugins that put
        // a popup at the cursor compute the cursor's screen cell (screenpos(),
        // screenline(), ...) and open an editor-anchored float there, which
        // belongs to the editor view the cursor is in. Decided when the float's
        // position is (re)configured and kept afterwards: moving the cursor
        // does not move a float that was placed at it.
        int cursorGrid = 0;
        int classifiedRow = -1;
        int classifiedCol = -1;
        // Whether the float sits in the cmdline area of Neovim's screen: a
        // plugin that completes a `:` command puts its menu right above the
        // cmdline, which is the row below the window's text. qnvim's editor
        // view has its cmdline widget there, so such a float is kept above it
        // (see placeFloats()).
        bool atCmdline = false;
        // Top left corner of the float on Neovim's screen, in cells. The screen
        // is grid 1, which qnvim sizes to the whole main window.
        int row = 0;
        int col = 0;
        // Neovim's rendering order of the floats: a float with a higher
        // compindex is drawn over one with a lower one.
        int compindex = 0;
        // Whether the float can receive mouse events.
        bool mouse = false;
        // Whether Neovim shows the float: win_float_pos shows it, win_hide
        // hides it without destroying it.
        bool visible = false;
        // The overlay, created once the size of the float's grid is known.
        QPointer<FloatWidget> widget;
    };

    // A highlight definition from Neovim (hl_attr_define), in the form the
    // float overlays are drawn with.
    struct HlAttr {
        // Invalid colors are Neovim's defaults (default_colors_set).
        QColor foreground;
        QColor background;
        int blend = 0;
        bool reverse = false;
        bool bold = false;
        bool italic = false;
        bool underline = false;
    };

    // The buffer of the editor, or of another editor that shares its document
    // (a document is shown by a single Neovim buffer).
    int bufferForEditor(Core::IEditor *) const;

    // The editor whose widget is `object` (or contains it).
    Core::IEditor *editorForWidget(QObject *object) const;

    // Qt Creator's "Enable text wrapping" setting changed: Neovim follows it
    // (see syncFromVim() for the other direction).
    void wrapSettingChanged();

    QWidget *editorViewWidget(Core::IEditor *) const;
    int windowForView(QWidget *) const;
    int windowForEditor(Core::IEditor *) const;
    // The Neovim window that shows the buffer but has no editor view yet, and
    // the editor that shows the buffer in a view that has no window yet.
    int windowWithoutView(int buffer) const;
    Core::IEditor *editorWithoutWindow(int buffer) const;

    // Tie a Neovim window to the editor its view shows.
    void bindWindow(int, Core::IEditor *);
    // Qt Creator -> Neovim: show an editor in the window that represents it.
    void attachEditorToWindow(Core::IEditor *);
    // Neovim -> Qt Creator: show a window's editor in its view.
    void activateWindowEditor(int);
    // Qt Creator -> Neovim: make an editor's window show its buffer, and be
    // the current one.
    void syncWindow(int, Core::IEditor *);
    // Neovim -> Qt Creator: make a window's view show its (changed) buffer.
    void updateWindowBuffer(int win, int buffer, const QString &filename,
                            const QString &bufferType, bool alwaysText);
    // Qt Creator -> Neovim: create a split (a window) for an editor view.
    void ensureWindowForView(Core::IEditor *);
    // Neovim -> Qt Creator: create a split for a window, showing its buffer.
    // The window is split off from `sourceWin` (0 for "whatever is current").
    // `sideBySide` is the orientation of the split, `before` tells whether the
    // window is placed left of / above its source window.
    void createViewForWindow(int win, int buffer, const QString &filename, int sourceWin,
                             bool sideBySide, bool before);
    // Neovim -> Qt Creator: open an editor for a buffer in the current view.
    Core::IEditor *openEditorForBuffer(int, const QString &, const QString &, bool);
    // Neovim -> Qt Creator: close the split of a window Neovim closed.
    void closeWindowForView(int);

    // Set up the connections that keep an editor's content, cursor and
    // selection in sync with Neovim.
    void setupEditor(Core::IEditor *);

    // Deferred work on Neovim's window autocommands: a window that is still new
    // once the command that created it was fully processed is a split.
    void reconcileWindows();
    // Close Neovim windows whose editor views were removed in Qt Creator.
    void closeWindowsForRemovedViews();

    // Neovim's floating windows (pickers, hover popups, ...) are shown as
    // overlays inside Qt Creator's main window (see qnvimfloat.h).
    //
    // Remember that Neovim draws in `grid`: the events of a grid can arrive
    // before the event that says what the grid is (win_float_pos for a float,
    // win_pos for a window), and a float's content must not be dropped then.
    void rememberGrid(int grid);
    // Whether the grid is one of the windows' grids, which draw documents that
    // Qt Creator shows itself.
    bool isWindowGrid(int grid) const;
    // The widget of a float, created when its grid is resized.
    FloatWidget *floatWidget(int grid);
    // Redraw the overlays: show the visible floats with a size, place them
    // where Neovim has them on the screen (which is the text area of the
    // current editor) and stack them in Neovim's order.
    void placeFloats();
    void setFloatGridSize(int grid, int rows, int cols);
    void setFloatGridLine(int grid, int row, int col, const QVariantList &cells);
    void scrollFloatGrid(int grid, int top, int bot, int left, int right, int rows, int cols);
    void clearFloatGrid(int grid);
    // Neovim closed the float (win_close) or its grid (grid_destroy).
    void closeFloat(int grid);
    // Forget every float and delete the overlays (Neovim is gone, or qnvim
    // shuts down).
    void clearFloats();
    // The cell a grid_line cell is drawn as: the colors and style come from the
    // cell's highlight and Neovim's defaults.
    FloatCell floatCell(int hlId, const QString &text) const;

    bool mEnabled = true;

    CmdLine *m_cmdLine = nullptr;
    // Parented to the editor it shows the numbers in; Qt Creator may destroy
    // that editor (and with it the column) before qnvim shuts down.
    QPointer<NumbersColumn> mNumbersColumn;
    NeovimQt::NeovimConnector *mNVim = nullptr;
    unsigned mVimChanges = 0;
    QMap<Core::IEditor *, int> mBuffers;
    QMap<int, Core::IEditor *> mEditors;
    QMap<int, bool> mChangedTicks;
    QMap<int, QString> mBufferType;
    // The buffer whose content mText holds (0 while it holds none).
    int mTextBuffer = 0;
    // Whether files opened by Neovim are restricted to Qt Creator text editors
    // (see g:QNVIM_always_text).
    bool mAlwaysText = true;

    // Neovim windows and the Qt Creator editor views that represent them.
    QMap<int, WindowState> mWindows;
    // Windows Neovim created that have no editor view yet: a window that is
    // still new once the command that created it was fully processed is a
    // split that needs one (see reconcileWindows()).
    QSet<int> mNewWindows;
    // Windows that qnvim closes, because their editor views were removed in
    // Qt Creator; no Qt Creator split is removed when Neovim reports them
    // closed.
    QSet<int> mClosingWindows;
    // The current Neovim window, as last seen through WinEnter.
    int mCurrentWindow = 0;
    // Whether the windows may have to be reconciled with Qt Creator's views.
    bool mWindowsDirty = true;
    // True while the external columns of the windows are being read from
    // Neovim (see refreshWindowSizes()).
    bool mRefreshingWindowSizes = false;
    // Qt Creator's "Enable text wrapping" setting as last seen; it is kept in
    // sync with Neovim's 'wrap' option (see syncFromVim()).
    bool mWrapSetting = false;

    // Neovim's floating windows, by the grid they are drawn in, and the
    // highlights their cells use (hl_attr_define id -> attributes).
    QMap<int, FloatState> mFloats;
    QMap<int, HlAttr> mHlAttrs;
    // The size every grid last reported (grid_resize, by grid id). A float's
    // grid can be reported before the win_float_pos that tells qnvim it is a
    // float: the size is remembered until the float's overlay is created.
    QMap<int, QSize> mGridSizes;

    // The editor view a Neovim window is being created for: the window that
    // appears for it is tied to that view (instead of being mistaken for a
    // window that still needs one).
    QPointer<QWidget> mCreatingWindowForView;
    // True while Qt Creator is being made to represent a Neovim window (Neovim
    // must not be told about the editors Qt Creator activates on the way).
    bool mInhibitWindowSync = false;
    // True while the split of a Neovim window is being removed: views that
    // lose their window must not be given a new one.
    bool mRemovingView = false;

    // Buffers whose content has not been pushed from Qt Creator to Neovim yet.
    // Such a buffer is empty to begin with (qnvim hijacks BufReadCmd), so reads
    // of that state must not be applied to the editor: the editor content may
    // just not have been pushed yet (it can still be loading from disk). The
    // buffer is dropped from this set as soon as its content was pushed (or read
    // back), from then on empty reads are genuine.
    QSet<int> mPendingPush;

    // True while editors are being closed because of a Neovim action: Neovim is
    // already deleting their buffer (`bd!` would fail with E516) or closed the
    // window that represents them (their buffers must be kept).
    bool mVimInitiatedClose = false;

    // The window a Qt Creator split is being created for, so that the editor
    // that ends up in it is not mistaken for one Neovim has to open a window
    // for.
    int mCreatingViewForWindow = 0;

    QString mText;
    int mHeight = 35;
    int mWidth = 80;
    QColor mForegroundColor = Qt::black;
    QColor mBackgroundColor = Qt::white;
    QColor mSpecialColor;
    QColor mCursorColor = Qt::white;
    bool mBusy = false;
    bool mMouse = false;
    bool mNumber = true;
    bool mRelativeNumber = true;
    bool mWrap = false;

    QByteArray mUIMode = "normal";
    QByteArray mMode = "n";

    int mSettingBufferFromVim = 0;
    unsigned long long mSyncCounter = 0;

    int mSavedCursorFlashTime = 0;

  signals:
};

} // namespace Internal
} // namespace QNVim
