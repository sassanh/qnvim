#pragma once

#include <QColor>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QSet>

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
class NumbersColumn;

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
    void editorOpened(Core::IEditor *);
    void editorAboutToClose(Core::IEditor *);

    QList<Core::IEditor *> editorsForBuffer(int) const;
    void closeEditorsForBuffer(int);

    void initializeBuffer(int);
    void detectFiletype(int);
    void handleNotification(const QByteArray &, const QVariantList &);
    void redraw(const QVariantList &);
    void updateCursorSize();

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
    };

    // The buffer of the editor, or of another editor that shares its document
    // (a document is shown by a single Neovim buffer).
    int bufferForEditor(Core::IEditor *) const;

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

    bool mEnabled = true;

    CmdLine *m_cmdLine = nullptr;
    NumbersColumn *mNumbersColumn = nullptr;
    NeovimQt::NeovimConnector *mNVim = nullptr;
    unsigned mVimChanges = 0;
    QMap<Core::IEditor *, int> mBuffers;
    QMap<int, Core::IEditor *> mEditors;
    QMap<int, bool> mChangedTicks;
    QMap<int, QString> mBufferType;
    // The buffer whose content mText holds (0 while it holds none).
    int mTextBuffer = 0;
    // Whether buffers without a file type are opened as plain text (see
    // g:QNVIM_always_text).
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
