// SPDX-FileCopyrightText: 2018-2019 Sassan Haradji <sassanh@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include "numbers_column.h"

#include <extensionsystem/iplugin.h>
#include <texteditor/plaintexteditorfactory.h>

#include <QColor>
#include <QPoint>
#include <QRect>

QT_BEGIN_NAMESPACE
class QPlainTextEdit;
QT_END_NAMESPACE

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

class NumbersColumn;

class QNVimPlugin : public ExtensionSystem::IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "QNVim.json")

  public:
    QNVimPlugin();
    ~QNVimPlugin();

    bool initialize(const QStringList &, QString *);
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

  private slots:
    // Save cursor flash time to variable instead of changing real value
    void saveCursorFlashTime(int cursorFlashTime);

  private:
    void initialize(bool reopen);
    void editorOpened(Core::IEditor *);
    void editorAboutToClose(Core::IEditor *);

    void initializeBuffer(int);
    void handleNotification(const QByteArray &, const QVariantList &);
    void redraw(const QVariantList &);
    void updateCursorSize();

    bool mEnabled = true;

    QPlainTextEdit *mCMDLine = nullptr;
    NumbersColumn *mNumbersColumn = nullptr;
    NeovimQt::NeovimConnector *mNVim = nullptr;
    unsigned mVimChanges = 0;
    QMap<Core::IEditor *, int> mBuffers;
    QMap<int, Core::IEditor *> mEditors;
    QMap<int, bool> mChangedTicks;
    QMap<int, QString> mBufferType;

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

    bool mCMDLineVisible = false;
    QString mCMDLineContent;
    QString mCMDLineDisplay;
    QString mMessageLineDisplay;
    int mCMDLinePos;
    QChar mCMDLineFirstc;
    QString mCMDLinePrompt;
    int mCMDLineIndent;

    QByteArray mUIMode = "normal";
    QByteArray mMode = "n";
    QPoint mCursor;
    QPoint mVCursor;

    int mSettingBufferFromVim = 0;
    unsigned long long mSyncCounter = 0;

    int mSavedCursorFlashTime = 0;
};

class HelpEditorFactory : public TextEditor::PlainTextEditorFactory {
    Q_OBJECT

  public:
    explicit HelpEditorFactory();
};

class TerminalEditorFactory : public TextEditor::PlainTextEditorFactory {
    Q_OBJECT

  public:
    explicit TerminalEditorFactory();
};

} // namespace Internal
} // namespace QNVim
