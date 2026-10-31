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
#include <QString>

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
    void syncCursorFromVim(int win, const QVariantList &, const QVariantList &,
                          QByteArray mode, int cursorVirtCol, int anchorVirtCol);
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
        // The view the window is represented by, and the connection that
        // notices the view being destroyed (Qt Creator removed the split): the
        // window is closed then. `viewGeneration` tells a view that was
        // rebound from one that was destroyed.
        QPointer<QWidget> view;
        QMetaObject::Connection viewDestroyed;
        int viewGeneration = 0;
        // Whether a view was ever bound to the window. A window without a view
        // either never had one, or its view was destroyed in Qt Creator (a null
        // view with `bound` set).
        bool bound = false;
        // The buffer shown in the window, as far as qnvim knows.
        int buffer = 0;
        // Cursor and visual anchor positions last synced with this window:
        // 1-based line and UTF-8 byte column (0 while nothing was synced).
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

    // Tie a Neovim window to the editor view that represents it, and to the
    // editor current in it.
    void bindWindow(int, Core::IEditor *);
    // Tie a Neovim window to an editor view before it shows an editor (the
    // view exists, its editor is still being opened). The editor is picked up
    // when it becomes current in the view. Returns whether the view was free.
    bool bindWindowToView(int, QWidget *);
    // Watch `view` for destruction: the window it represents is closed then.
    void watchView(int, QWidget *);
    // Drop the view of a window without closing it (the view is already gone
    // or no longer represents the window).
    void unbindWindow(int);
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
    // `sourceWin` (0 for "whatever is current") and `split` ("left", "right",
    // "above", "below") say where; empty `split` picks the orientation from
    // Qt Creator's layout.
    void ensureWindowForView(Core::IEditor *, int sourceWin = 0,
                             const QByteArray &split = QByteArray());
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

    // Neovim -> Qt Creator: ask Neovim for the matches of the current search
    // pattern in the visible buffers, then show them in their editors (see
    // applySearchHighlights()).
    void refreshSearchHighlights();
    // Whether search matches are to be shown: a search is being typed, or
    // 'hlsearch' and v:hlsearch are on and there is a pattern.
    bool searchHighlightsEnabled() const;
    // Show the search matches of each editor's buffer as extra selections, with
    // the colors Qt Creator uses for its own search results, or clear them when
    // there is nothing to highlight.
    void applySearchHighlights();
    // The buffers shown by the windows that have an editor; search highlights
    // are only computed for those.
    QList<int> visibleBuffers() const;

    // Deferred work on Neovim's window autocommands: a window that is still new
    // once the command that created it was fully processed is a split.
    void reconcileWindows();

    // Whether the grid is one of the windows' grids, which draw documents that
    // Qt Creator shows itself: their redraw events are not forwarded to the
    // float layer.
    bool isWindowGrid(int grid) const;

    // The position and external columns of the windows that have a grid and an
    // editor, as the float layer needs them to place the floats anchored to
    // the windows (see FloatLayer::place()).
    QList<FloatLayer::WindowAnchor> windowAnchors() const;

    // Place the float overlays (see FloatLayer::place()).
    void placeFloats();
    // Ask the user's float filter (qnvim.float_filter()) about a floating
    // window, and place the floats again when the answer arrives.
    void queryFloatFilter(int grid, int win);

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

    // A match of the last search pattern, as QNVimSearchMatches (see
    // src/preamble.vim) reports it: a 1-based line and UTF-8 byte columns
    // (the end column is exclusive).
    struct SearchMatch {
        int line = 0;
        int startColumn = 0;
        int endColumn = 0;
    };
    // The matches computed for each buffer, and what they were computed for;
    // a change in any of these asks Neovim for the matches again.
    QMap<int, QList<SearchMatch>> mSearchMatches;
    QString mSearchPattern;
    // The pattern being typed in a / or ? command line while 'incsearch' and
    // 'hlsearch' preview it; empty when no search is being typed, so that
    // mSearchPattern is used.
    QString mSearchPreviewPattern;
    // Whether the matches are to be shown at all: 'hlsearch' and v:hlsearch
    // are on and there is a pattern. ('hlsearch' can be off while a pattern
    // from an earlier search is still remembered.)
    bool mSearchEnabled = false;
    bool mSearchIgnoreCase = false;
    bool mSearchSmartCase = false;
    // The buffers whose editors were given search selections, so that clearing
    // or refreshing them does not touch every open editor (each keystroke
    // refreshes the matches, and there can be many files open).
    QSet<int> mSearchHighlighted;
    // Bumped for every request; answers to an older request are dropped.
    unsigned long long mSearchCounter = 0;

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

    // The overlays of Neovim's floating windows (see qnvimfloat.h): fed with
    // the redraw events of the grids that are not windows' grids, and placed
    // once Neovim has flushed a redraw.
    FloatLayer mFloatLayer;
    // Whether the user registered a float filter (qnvim.float_filter()): only
    // then are the floats' windows asked about (see queryFloatFilter()).
    bool mFloatFilterRegistered = false;
    // The grid of every floating window Neovim reported (win_float_pos).
    QMap<int, int> mFloatGrids;
    // The filter's answer for a floating window: whether the float is placed in
    // the current editor's view (see FloatLayer::setFloatInEditorArea()).
    QMap<int, bool> mFloatFilters;

    // The editor views a Neovim window is being created for (see
    // ensureWindowForView()): reconciliation waits for them, so a window that
    // is on its way is not mistaken for one that still needs a view.
    QSet<QWidget *> mPendingWindowViews;
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
    // Neovim's default colors (default_colors_set): the current editor's
    // palette is updated with them, and they are forwarded to the float layer,
    // which draws the floats with them (see redrawEvent()).
    QColor mForegroundColor = Qt::black;
    QColor mBackgroundColor = Qt::white;
    QColor mCursorColor = Qt::white;
    bool mBusy = false;
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
