#pragma once

#include "qnvim_global.h"
#include <QColor>
#include <QPoint>
#include <QRect>
#include <QWidget>
#include <QMutex>

#include <extensionsystem/iplugin.h>

class QPlainTextEdit;

namespace Core {
class IEditor;
}

namespace TextEditor {
class TextEditorWidget;
}

namespace NeovimQt {
class NeovimConnector;
class InputConv;
}

namespace QNVim {
namespace Internal {

class NumbersColumn : public QWidget
{
    Q_OBJECT
    bool mNumber;
    TextEditor::TextEditorWidget *mEditor;

public:
    NumbersColumn();

    void setEditor(TextEditor::TextEditorWidget *);
    void setNumber(bool);

protected:
    void paintEvent(QPaintEvent *event);
    bool eventFilter(QObject *, QEvent *);

private:
    void updateGeometry();
};

class QNVimPlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "QNVim.json")

public:
    QNVimPlugin();
    ~QNVimPlugin();

    bool initialize(const QStringList &, QString *);
    bool initialize();
    void extensionsInitialized();
    ShutdownFlag aboutToShutdown();

    bool eventFilter(QObject *, QEvent *);
    void toggleQNVim();

protected:
    QString filename(Core::IEditor * = NULL) const;

    void fixSize(Core::IEditor * = NULL);
    void syncCursorToVim(Core::IEditor * = NULL, bool = false);
    void syncSelectionToVim(Core::IEditor * = NULL, bool = false);
    void syncModifiedToVim(Core::IEditor * = NULL);
    void syncToVim(Core::IEditor * = NULL, bool = false, std::function<void()> = NULL);
    void syncFromVim(bool = false);

    void triggerCommand(const QByteArray &);

private:


    void editorOpened(Core::IEditor *);
    void editorAboutToClose(Core::IEditor *);

    void handleNotification(const QByteArray &, const QVariantList &);
    void redraw(const QVariantList &);

    bool mEnabled;

    QMutex mSyncMutex;

    QPlainTextEdit *mCMDLine;
    NumbersColumn *mNumbersColumn;
    NeovimQt::NeovimConnector *mNVim;
    NeovimQt::InputConv *mInputConv;
    unsigned mVimChanges;
    QMap<QString, Core::IEditor *> mEditors;
    QMap<QString, unsigned long long> mBuffers;
    QMap<QString, bool> mInitialized;

    QString mText;
    unsigned mWidth, mHeight;
    QColor mForegroundColor, mBackgroundColor, mSpecialColor;
    QColor mCursorColor;
    bool mBusy, mMouse, mNumber, mRelativeNumber, mWrap;

    bool mCMDLineVisible;
    QString mCMDLineContent, mCMDLineDisplay;
    unsigned mCMDLinePos;
    QChar mCMDLineFirstc;
    QString mCMDLinePrompt;
    unsigned mCMDLineIndent;

    QByteArray mUIMode, mMode;
    QPoint mCursor, mVCursor;

    QRect mScrollRegion;
};

} // namespace Internal
} // namespace QNVim
