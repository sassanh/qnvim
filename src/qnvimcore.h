#pragma once

#include <QColor>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPoint>
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
    void syncCursorFromVim(const QVariantList &, const QVariantList &, QByteArray mode);
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

    bool mEnabled = true;

    CmdLine *m_cmdLine = nullptr;
    NumbersColumn *mNumbersColumn = nullptr;
    NeovimQt::NeovimConnector *mNVim = nullptr;
    unsigned mVimChanges = 0;
    QMap<Core::IEditor *, int> mBuffers;
    QMap<int, Core::IEditor *> mEditors;
    QMap<int, bool> mChangedTicks;
    QMap<int, QString> mBufferType;

    // Buffers whose content has not been pushed from Qt Creator to Neovim yet.
    // Such a buffer is empty to begin with (qnvim hijacks BufReadCmd), so reads
    // of that state must not be applied to the editor: the editor content may
    // just not have been pushed yet (it can still be loading from disk). The
    // buffer is dropped from this set as soon as its content was pushed (or read
    // back), from then on empty reads are genuine.
    QSet<int> mPendingPush;

    // True while editors are being closed because Neovim deleted their buffer:
    // the buffer is already being deleted there, so it must not be `bd!`-ed again.
    bool mVimInitiatedClose = false;

    QString mText;
    int mWidth = 80;
    int mHeight = 35;
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
    QPoint mCursor;
    QPoint mVCursor;

    // Top line of Neovim's window viewport (1-based, 0 while unknown)
    int mNvimTopline = 0;
    // Top line Neovim scrolled to, to be applied to the editor viewport (0 when there is none)
    int mPendingTopline = 0;

    int mSettingBufferFromVim = 0;
    unsigned long long mSyncCounter = 0;

    int mSavedCursorFlashTime = 0;

  signals:
};

} // namespace Internal
} // namespace QNVim
