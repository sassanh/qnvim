#pragma once

#include "qnvim_global.h"
#include <QColor>
#include <QPoint>
#include <QRect>
#include <QWidget>
#include <QMutex>

#include <extensionsystem/iplugin.h>
#include <texteditor/plaintexteditorfactory.h>

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

class QNVimPlugin : public ExtensionSystem::IPlugin {
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
    QString filename(Core::IEditor * = nullptr) const;

    void fixSize(Core::IEditor * = nullptr);
    void syncCursorToVim(Core::IEditor * = nullptr);
    void syncSelectionToVim(Core::IEditor * = nullptr);
    void syncModifiedToVim(Core::IEditor * = nullptr);
    void syncToVim(Core::IEditor * = nullptr, std::function<void()> = nullptr);
    void syncCursorFromVim(const QVariantList &, const QVariantList &, QByteArray mode);
    void syncFromVim();

    void triggerCommand(const QByteArray &);

private:
    void editorOpened(Core::IEditor *);
    void editorAboutToClose(Core::IEditor *);

    void initializeBuffer(long, QString);
    void handleNotification(const QByteArray &, const QVariantList &);
    void redraw(const QVariantList &);

    bool mEnabled;

    QMutex mSyncMutex;

    QPlainTextEdit *mCMDLine;
    NumbersColumn *mNumbersColumn;
    NeovimQt::NeovimConnector *mNVim;
    NeovimQt::InputConv *mInputConv;
    unsigned mVimChanges;
    QMap<QString, int> mBuffers;
    QMap<QString, Core::IEditor *> mEditors;
    QMap<int, QString> mFilenames;
    QMap<QString, bool> mInitialized;
    QMap<int, bool> mChangedTicks;

    QString mText;
    int mWidth, mHeight;
    QColor mForegroundColor, mBackgroundColor, mSpecialColor;
    QColor mCursorColor;
    bool mBusy, mMouse, mNumber, mRelativeNumber, mWrap;

    bool mCMDLineVisible;
    QString mCMDLineContent, mCMDLineDisplay;
    int mCMDLinePos;
    QChar mCMDLineFirstc;
    QString mCMDLinePrompt;
    int mCMDLineIndent;

    QByteArray mUIMode, mMode;
    QPoint mCursor, mVCursor;

    QRect mScrollRegion;
    bool settingBufferFromVim;
    unsigned long long mSyncCounter;
};

class TerminalEditorFactory : public TextEditor::PlainTextEditorFactory {
    Q_OBJECT

public:
    explicit TerminalEditorFactory();
};

} // namespace Internal
} // namespace QNVim
