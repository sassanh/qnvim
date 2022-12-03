// SPDX-FileCopyrightText: 2018-2019 Sassan Haradji <sassanh@gmail.com>
// SPDX-License-Identifier: MIT

#include "qnvimplugin.h"
#include "numbers_column.h"
#include "qnvimconstants.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/icontext.h>
#include <coreplugin/statusbarmanager.h>
#include <gui/input.h>
#include <msgpackrequest.h>
#include <neovimconnector.h>
#include <projectexplorer/project.h>
#include <projectexplorer/session.h>
#include <texteditor/displaysettings.h>
#include <texteditor/fontsettings.h>
#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>
#include <texteditor/texteditorsettings.h>
#include <utils/differ.h>
#include <utils/fancylineedit.h>
#include <utils/fileutils.h>
#include <utils/osspecificaspects.h>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTextBlock>
#include <QTextEdit>
#include <QThread>
#include <QtMath>

namespace QNVim {
namespace Internal {

QNVimPlugin::QNVimPlugin() {
}

QNVimPlugin::~QNVimPlugin() {
    if (mCMDLine)
        Core::StatusBarManager::destroyStatusBarWidget(mCMDLine);

    if (mNVim)
        mNVim->deleteLater();
}

QString QNVimPlugin::filename(Core::IEditor *editor) const {
    if (!editor)
        return QString();

    auto filename = editor->document()->filePath().toString();
    if (filename.isEmpty())
        filename = editor->document()->displayName();

    return filename;
}

void QNVimPlugin::fixSize(Core::IEditor *editor) {
    if (!editor) {
        return;
    }

    if (!mNVim or !mNVim->isReady())
        return;

    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    QFontMetricsF fm(textEditor->textDocument()->fontSettings().font());

    // -1 is for the visual whitespaces that Qt Creator adds (whether it renders them or not)
    // TODO: after ext_columns is implemented in neovim +6 should be removed
    const int width = qFloor(textEditor->viewport()->width() / fm.width('A')) - 1 + 6;
    const int height = qFloor(textEditor->height() / fm.lineSpacing());

    if (width != mWidth or height != mHeight)
        mNVim->api6()->nvim_ui_try_resize_grid(1, width, height);
}

void QNVimPlugin::syncCursorToVim(Core::IEditor *editor) {
    if (!editor)
        editor = Core::EditorManager::currentEditor();

    if (!editor or !mBuffers.contains(editor))
        return;

    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());

    if (mMode == "v" or mMode == "V" or mMode == "\x16" or
        textEditor->textCursor().position() != textEditor->textCursor().anchor())
        return;

    const auto text = textEditor->toPlainText();
    int cursorPosition = textEditor->textCursor().position();
    int line = text.leftRef(cursorPosition).count('\n') + 1;
    int col = text.left(cursorPosition).section('\n', -1).toUtf8().length() + 1;

    if (line == mCursor.y() and col == mCursor.x()) {
        return;
    }

    mCursor.setY(line);
    mCursor.setX(col);
    mNVim->api2()->nvim_command(QStringLiteral("buffer %1|call SetCursor(%2,%3)").arg(mBuffers[editor]).arg(line).arg(col).toUtf8());
}

void QNVimPlugin::syncSelectionToVim(Core::IEditor *editor) {
    if (!editor)
        editor = Core::EditorManager::currentEditor();

    if (!editor or !mBuffers.contains(editor))
        return;

    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    QString text = textEditor->toPlainText();
    auto cursor = textEditor->hasBlockSelection() ? textEditor->blockSelection() : textEditor->textCursor();
    int cursorPosition = cursor.position();
    int anchorPosition = cursor.anchor();
    int line, col, vLine, vCol;

    if (anchorPosition == cursorPosition)
        return;

    QString visualCommand;
    if (textEditor->hasBlockSelection()) {
        line = text.leftRef(cursorPosition).count('\n') + 1;
        col = text.left(cursorPosition).section('\n', -1).length() + 1;
        vLine = text.leftRef(anchorPosition).count('\n') + 1;
        vCol = text.left(anchorPosition).section('\n', -1).length() + 1;

        if (vCol < col)
            --col;
        else if (vCol > col)
            --vCol;

        visualCommand = "\x16";
    } else if (mMode == "V") {
        return;
    } else {
        if (anchorPosition < cursorPosition)
            --cursorPosition;
        else
            --anchorPosition;

        line = text.leftRef(cursorPosition).count('\n') + 1;
        col = text.left(cursorPosition).section('\n', -1).length() + 1;
        vLine = text.leftRef(anchorPosition).count('\n') + 1;
        vCol = text.left(anchorPosition).section('\n', -1).length() + 1;
        visualCommand = "v";
    }

    if (line == mCursor.y() and col == mCursor.x() and vLine == mVCursor.y() and vCol == mVCursor.x())
        return;

    mCursor.setY(line);
    mCursor.setX(col);
    mVCursor.setY(vLine);
    mVCursor.setX(vCol);
    mNVim->api2()->nvim_command(QStringLiteral("buffer %1|normal! \x03%3G%4|%2%5G%6|")
                                                  .arg(mBuffers[editor])
                                                  .arg(visualCommand)
                                                  .arg(vLine)
                                                  .arg(vCol)
                                                  .arg(line)
                                                  .arg(col).toUtf8());
}

void QNVimPlugin::syncToVim(Core::IEditor *editor, std::function<void()> callback) {
    if (!editor)
        editor = Core::EditorManager::currentEditor();

    if (!editor or !mBuffers.contains(editor))
        return;

    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    QString text = textEditor->toPlainText();
    int cursorPosition = textEditor->textCursor().position();
    int line = text.leftRef(cursorPosition).count('\n') + 1;
    int col = text.left(cursorPosition).section('\n', -1).toUtf8().length() + 1;

    if (mText != text) {
        int bufferNumber = mBuffers[editor];
        auto request = mNVim->api2()->nvim_buf_set_lines(bufferNumber, 0, -1, true, text.toUtf8().split('\n'));
        connect(request, &NeovimQt::MsgpackRequest::finished, this, [=]() {
            connect(mNVim->api2()->nvim_command(QStringLiteral("call cursor(%1,%2)").arg(line).arg(col).toUtf8()),
                    &NeovimQt::MsgpackRequest::finished, [=]() {
                        if (callback)
                            callback();
                    });
        });
    } else if (callback)
        callback();
}

void QNVimPlugin::syncCursorFromVim(const QVariantList &pos, const QVariantList &vPos, QByteArray mode) {
    if (!mEnabled)
        return;

    auto editor = Core::EditorManager::currentEditor();
    if (!editor or !mBuffers.contains(editor))
        return;

    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    int line = pos[0].toInt();
    int col = pos[1].toInt();
    col = QString::fromUtf8(mText.section('\n', line - 1, line - 1).toUtf8().left(col - 1)).length() + 1;

    int vLine = vPos[0].toInt();
    int vCol = vPos[1].toInt();
    vCol = QString::fromUtf8(mText.section('\n', vLine - 1, vLine - 1).toUtf8().left(vCol)).length();

    mMode = mode;
    mCursor.setY(line);
    mCursor.setX(col);
    mVCursor.setY(vLine);
    mVCursor.setX(vCol);

    int a = QString("\n" + mText).section('\n', 0, vLine - 1).length() + vCol - 1;
    int p = QString("\n" + mText).section('\n', 0, line - 1).length() + col - 1;
    if (mMode == "V") {
        if (a < p) {
            a = QString("\n" + mText).section('\n', 0, vLine - 1).length();
            p = QString("\n" + mText).section('\n', 0, line).length() - 1;
        } else {
            a = QString("\n" + mText).section('\n', 0, vLine).length() - 1;
            p = QString("\n" + mText).section('\n', 0, line - 1).length();
        }

        QTextCursor cursor = textEditor->textCursor();
        cursor.setPosition(a);
        cursor.setPosition(p, QTextCursor::KeepAnchor);

        if (textEditor->textCursor().anchor() != cursor.anchor() or
            textEditor->textCursor().position() != cursor.position())
            textEditor->setTextCursor(cursor);

    } else if (mMode == "v") {
        if (a > p)
            ++a;
        else
            ++p;

        QTextCursor cursor = textEditor->textCursor();
        cursor.setPosition(a);
        cursor.setPosition(p, QTextCursor::KeepAnchor);

        if (textEditor->textCursor().anchor() != cursor.anchor() or
            textEditor->textCursor().position() != cursor.position())
            textEditor->setTextCursor(cursor);
    } else if (mMode == "\x16") {
        if (vCol > col)
            ++a;
        else
            ++p;

        QTextCursor cursor = textEditor->textCursor();
        cursor.setPosition(a);
        cursor.setPosition(p, QTextCursor::KeepAnchor);
        textEditor->setBlockSelection(cursor);
    } else {
        QTextCursor cursor = textEditor->textCursor();
        cursor.clearSelection();
        cursor.setPosition(p);

        if (textEditor->textCursor().position() != cursor.position() or
            textEditor->textCursor().hasSelection())
            textEditor->setTextCursor(cursor);
    }
}

void QNVimPlugin::syncFromVim() {
    if (!mEnabled)
        return;

    auto editor = Core::EditorManager::currentEditor();

    if (!editor or !mBuffers.contains(editor))
        return;

    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    unsigned long long syncCoutner = ++mSyncCounter;

    auto request = mNVim->api2()->nvim_eval("[bufnr(''), b:changedtick, mode(1), &modified, getpos('.'), getpos('v'), &number, &relativenumber, &wrap]");
    connect(request, &NeovimQt::MsgpackRequest::finished, this, [=](quint32, quint64, const QVariant &v) {
        QVariantList state = v.toList();

        if (mSyncCounter != syncCoutner)
            return;

        if (!mBuffers.contains(editor)) {
            return;
        }

        int bufferNumber = mBuffers[editor];
        if (state[0].toString().toLong() != bufferNumber)
            return;

        unsigned long long changedtick = state[1].toULongLong();
        QByteArray mode = state[2].toByteArray();
        bool modified = state[3].toBool();
        QVariantList pos = state[4].toList().mid(1, 2);
        QVariantList vPos = state[5].toList().mid(1, 2);

        mNumber = state[6].toBool();
        mRelativeNumber = state[7].toBool();
        mWrap = state[8].toBool();
        mNumbersColumn->setNumber(mNumber);
        mNumbersColumn->setEditor(mRelativeNumber ? textEditor : nullptr);

        if (textEditor->wordWrapMode() != (mWrap ? QTextOption::WrapAnywhere : QTextOption::NoWrap))
            textEditor->setWordWrapMode(mWrap ? QTextOption::WrapAnywhere : QTextOption::NoWrap);

        if (mChangedTicks.value(bufferNumber, 0) == changedtick) {
            syncCursorFromVim(pos, vPos, mode);
            return;
        }

        mChangedTicks[bufferNumber] = changedtick;

        qWarning() << "QNVimPlugin::syncFromVim";

        auto request = mNVim->api2()->nvim_buf_get_lines(bufferNumber, 0, -1, true);
        connect(request, &NeovimQt::MsgpackRequest::finished, this, [=](quint32, quint64, const QVariant &lines) {
            if (!mBuffers.contains(editor)) {
                return;
            }

            mText.clear();
            auto linesList = lines.toList();
            for (const auto &t : linesList)
                mText += QString::fromUtf8(t.toByteArray()) + '\n';
            mText.chop(1);

            QString oldText = textEditor->toPlainText();

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
                        qWarning() << 3 << d.text << d.text.size() << d.text.length();
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
                        qWarning() << 2 << d.text << d.text.size() << d.text.length();
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
                        qWarning() << 1 << d.text << d.text.size() << d.text.length();
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

            syncCursorFromVim(pos, vPos, mode);
        });
    });
}

void QNVimPlugin::triggerCommand(const QByteArray &commandId) {
    Core::ActionManager::command(commandId.constData())->action()->trigger();
}

void QNVimPlugin::saveCursorFlashTime(int cursorFlashTime) {
    mSavedCursorFlashTime = cursorFlashTime;

    disconnect(QApplication::styleHints(), &QStyleHints::cursorFlashTimeChanged, this, &QNVimPlugin::saveCursorFlashTime);
    QApplication::setCursorFlashTime(0);
    connect(QApplication::styleHints(), &QStyleHints::cursorFlashTimeChanged, this, &QNVimPlugin::saveCursorFlashTime);
}

bool QNVimPlugin::initialize(const QStringList &arguments, QString *errorString) {
    Q_UNUSED(arguments)
    Q_UNUSED(errorString)

    new HelpEditorFactory();
    new TerminalEditorFactory();

    auto action = new QAction(tr("Toggle QNVim"), this);
    Core::Command *cmd = Core::ActionManager::registerAction(action, Constants::TOGGLE_ID,
                                                             Core::Context(Core::Constants::C_GLOBAL));
    cmd->setDefaultKeySequence(QKeySequence(tr("Alt+Shift+V,Alt+Shift+V")));
    connect(action, &QAction::triggered, this, &QNVimPlugin::toggleQNVim);

    Core::ActionContainer *menu = Core::ActionManager::createMenu(Constants::MENU_ID);
    menu->menu()->setTitle(tr("QNVim"));
    menu->addAction(cmd);
    Core::ActionManager::actionContainer(Core::Constants::M_TOOLS)->addMenu(menu);

    qunsetenv("NVIM_LISTEN_ADDRESS");

    initialize(false);

    return true;
}

void QNVimPlugin::extensionsInitialized() {
    // Retrieve objects from the plugin manager's object pool
    // In the extensionsInitialized function, a plugin can be sure that all
    // plugins that depend on it are completely initialized.
}

ExtensionSystem::IPlugin::ShutdownFlag QNVimPlugin::aboutToShutdown() {
    mEnabled = false;
    // Save settings
    // Disconnect from signals that are not needed during shutdown
    // Hide UI (if you add UI that is not in the main window directly)
    return SynchronousShutdown;
}

bool QNVimPlugin::eventFilter(QObject *object, QEvent *event) {
    if (!mEnabled)
        return false;
    /* if (qobject_cast<QLabel *>(object)) */
    if (qobject_cast<TextEditor::TextEditorWidget *>(object) or qobject_cast<QPlainTextEdit *>(object)) {
        if (event->type() == QEvent::Resize) {
            QTimer::singleShot(100, this, [=]() { fixSize(); });
            return false;
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

void QNVimPlugin::toggleQNVim() {
    qWarning() << "QNVimPlugin::toggleQNVim";
    mEnabled = !mEnabled;

    if (mEnabled) {
        initialize(true);
    } else {
        qobject_cast<QWidget *>(mCMDLine->parentWidget()->children()[2])->show();
        mCMDLine->deleteLater();

        disconnect(QApplication::styleHints(), &QStyleHints::cursorFlashTimeChanged, this, &QNVimPlugin::saveCursorFlashTime);
        QApplication::setCursorFlashTime(mSavedCursorFlashTime);

        mNumbersColumn->deleteLater();
        auto request = mNVim->api2()->nvim_command("q!");
        connect(request, &NeovimQt::MsgpackRequest::finished, this, [=]() {
            mNVim->deleteLater();
            mNVim = nullptr;
        });
        disconnect(Core::EditorManager::instance(), &Core::EditorManager::editorAboutToClose,
                   this, &QNVimPlugin::editorAboutToClose);
        disconnect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged,
                   this, &QNVimPlugin::editorOpened);
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

            TextEditor::TextEditorWidget *textEditor = qobject_cast<TextEditor::TextEditorWidget *>(widget);
            textEditor->setCursorWidth(1);
            widget->removeEventFilter(this);
            mEditors.remove(key);
        }
        mBuffers.clear();
        mChangedTicks.clear();
        mBufferType.clear();
    }
}

void QNVimPlugin::initialize(bool reopen) {
    qWarning() << "QNVimPlugin::initialize";
    mCMDLine = new QPlainTextEdit;
    Core::StatusBarManager::addStatusBarWidget(mCMDLine, Core::StatusBarManager::First);
    mCMDLine->document()->setDocumentMargin(0);
    mCMDLine->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mCMDLine->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mCMDLine->setLineWrapMode(QPlainTextEdit::NoWrap);
    mCMDLine->setMinimumWidth(200);
    mCMDLine->setFocusPolicy(Qt::StrongFocus);
    mCMDLine->installEventFilter(this);
    mCMDLine->setFont(TextEditor::TextEditorSettings::instance()->fontSettings().font());

    qobject_cast<QWidget *>(mCMDLine->parentWidget()->children()[2])->hide();

    saveCursorFlashTime(QApplication::cursorFlashTime());

    connect(Core::EditorManager::instance(), &Core::EditorManager::editorAboutToClose,
            this, &QNVimPlugin::editorAboutToClose);
    connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged,
            this, &QNVimPlugin::editorOpened);

    mNumbersColumn = new NumbersColumn();
    mNVim = NeovimQt::NeovimConnector::spawn({"--cmd", "let g:QNVIM=1"});

    connect(mNVim, &NeovimQt::NeovimConnector::ready, this, [=]() {
        mNVim->api2()->nvim_command(QStringLiteral("\
let g:QNVIM_always_text=v:true\n\
let g:neovim_channel=%1\n\
execute \"command -bar Build call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Build')\"\n\
execute \"command -bar BuildProject call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Build')\"\n\
execute \"command -bar BuildAll call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.BuildSession')\"\n\
execute \"command -bar Rebuild call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Rebuild')\"\n\
execute \"command -bar RebuildProject call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Rebuild')\"\n\
execute \"command -bar RebuildAll call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.RebuildSession')\"\n\
execute \"command -bar Clean call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Clean')\"\n\
execute \"command -bar CleanProject call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Clean')\"\n\
execute \"command -bar CleanAll call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.CleanSession')\"\n\
execute \"command -bar Deploy call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Deploy')\"\n\
execute \"command -bar DeployProject call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Deploy')\"\n\
execute \"command -bar DeployAll call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.DeploySession')\"\n\
execute \"command -bar Run call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Run')\"\n\
execute \"command -bar Debug call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Debug')\"\n\
execute \"command -bar DebugStart call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Debug')\"\n\
execute \"command -bar DebugContinue call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.Continue')\"\n\
execute \"command -bar QMake call rpcnotify(%1, 'Gui', 'triggerCommand', 'Qt4Builder.RunQMake')\"\n\
execute \"command -bar Target call rpcnotify(%1, 'Gui', 'triggerCommand', 'ProjectExplorer.SelectTargetQuick')\"\n\
\
execute \"autocmd BufReadCmd * :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'BufReadCmd', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden, g:QNVIM_always_text)\"\n\
execute \"autocmd TermOpen * :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'TermOpen', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden, g:QNVIM_always_text)\"\n\
execute \"autocmd BufWriteCmd * :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'BufWriteCmd', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden, g:QNVIM_always_text)|set nomodified\"\n\
execute \"autocmd BufEnter * nested :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'BufEnter', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden, g:QNVIM_always_text)\"\n\
execute \"autocmd BufDelete * nested :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'BufDelete', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden, g:QNVIM_always_text)\"\n\
execute \"autocmd BufHidden * nested :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'BufHidden', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden, g:QNVIM_always_text)\"\n\
execute \"autocmd BufWipeout * nested :call rpcnotify(%1, 'Gui', 'fileAutoCommand', 'BufWipeout', expand('<abuf>'), expand('<afile>:p'), &buftype, &buflisted, &bufhidden, g:QNVIM_always_text)\"\n\
execute \"autocmd FileType help set modifiable|read <afile>|set nomodifiable\"\n\
\
function! SetCursor(line, col)\n\
    call cursor(a:line, a:col)\n\
    if mode()[0] ==# 'i' or mode()[0] ==# 'R'\n\
        normal! i\x07u\x03\n\
    endif\n\
    call cursor(a:line, a:col)\n\
endfunction\n\
autocmd VimEnter * let $MYQVIMRC=substitute($MYVIMRC, 'init.vim$', 'qnvim.vim', v:true) | source $MYQVIMRC")
                                                      .arg(mNVim->channel()).toUtf8());
        connect(mNVim->api2(), &NeovimQt::NeovimApi2::neovimNotification,
                this, &QNVimPlugin::handleNotification);

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
        request->setTimeout(2000);
        connect(request, &NeovimQt::MsgpackRequest::timeout, mNVim, &NeovimQt::NeovimConnector::fatalTimeout);
        connect(request, &NeovimQt::MsgpackRequest::timeout, [=]() {
            qWarning() << "Neovim: Connection timed out!";
        });
        connect(request, &NeovimQt::MsgpackRequest::finished, this, [=]() {
            qWarning() << "Neovim: attached!";
            if (reopen)
                QNVimPlugin::editorOpened(Core::EditorManager::currentEditor());
        });

        mNVim->api2()->nvim_subscribe("Gui");
        mNVim->api2()->nvim_subscribe("api-buffer-updates");
    });
}

void QNVimPlugin::editorOpened(Core::IEditor *editor) {
    if (!mEnabled)
        return;

    if (!editor)
        return;

    QString filename(this->filename(editor));
    mText.clear();
    qWarning() << "Opened " << filename << mSettingBufferFromVim;

    QWidget *widget = editor->widget();
    if (!widget)
        return;

    auto project = ProjectExplorer::SessionManager::projectForFile(
        Utils::FilePath::fromString(filename));
    qWarning() << project;
    if (project) {
        QString projectDirectory = project->projectDirectory().toString();
        if (!projectDirectory.isEmpty())
            mNVim->api2()->nvim_command(QStringLiteral("cd %1").arg(projectDirectory).toUtf8());
    }

    if (!qobject_cast<TextEditor::TextEditorWidget *>(widget)) {
        mNumbersColumn->setEditor(nullptr);
        return;
    }
    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());

    if (mBuffers.contains(editor)) {
        if (!mSettingBufferFromVim)
            mNVim->api2()->nvim_command(QStringLiteral("buffer %1").arg(mBuffers[editor]).toUtf8());
    } else {
        if (mNVim and mNVim->isReady()) {
            if (mSettingBufferFromVim > 0) {
                mBuffers[editor] = mSettingBufferFromVim;
                mEditors[mSettingBufferFromVim] = editor;
                initializeBuffer(mSettingBufferFromVim);
            } else {
                QString f = filename;
                if (f.contains('\\') or f.contains('\'') or f.contains('"') or f.contains(' ')) {
                    static const auto regExp = QRegularExpression("[\\\"' ]");
                    f = '"' + f.replace(regExp, "\\\1") + '"';
                }

                auto request = mNVim->api2()->nvim_command(QStringLiteral("e %1").arg(f).toUtf8());
                connect(request, &NeovimQt::MsgpackRequest::finished, this, [=]() {
                    auto request = mNVim->api2()->nvim_eval(QStringLiteral("bufnr('')").toUtf8());
                    connect(request, &NeovimQt::MsgpackRequest::finished, this, [=](quint32, quint64, const QVariant &v) {
                        mBuffers[editor] = v.toInt();
                        mEditors[v.toInt()] = editor;
                        initializeBuffer(v.toInt());
                    });
                });
            }
        }

        Core::IDocument *document = editor->document();

        connect(
            document, &Core::IDocument::contentsChanged, this, [=]() {
                auto buffer = mBuffers[editor];
                QString bufferType = mBufferType[buffer];
                if (!mEditors.contains(buffer) or (bufferType != "acwrite" and !bufferType.isEmpty()))
                    return;
                syncToVim(editor);
            },
            Qt::QueuedConnection);
        connect(
            textEditor, &TextEditor::TextEditorWidget::cursorPositionChanged, this, [=]() {
                if (Core::EditorManager::currentEditor() != editor)
                    return;
                QString newText = textEditor->toPlainText();
                if (newText != mText)
                    return;
                syncCursorToVim(editor);
            },
            Qt::QueuedConnection);
        connect(
            textEditor, &TextEditor::TextEditorWidget::selectionChanged, this, [=]() {
                if (Core::EditorManager::currentEditor() != editor)
                    return;
                QString newText = textEditor->toPlainText();
                if (newText != mText)
                    return;
                syncSelectionToVim(editor);
            },
            Qt::QueuedConnection);
        connect(textEditor->textDocument(), &TextEditor::TextDocument::fontSettingsChanged, this, &QNVimPlugin::updateCursorSize);
    }
    mSettingBufferFromVim = 0;

    mNumbersColumn->setEditor(textEditor);

    widget->setAttribute(Qt::WA_KeyCompression, false);
    widget->installEventFilter(this);

    QTimer::singleShot(100, this, [=]() { fixSize(editor); });
}

void QNVimPlugin::editorAboutToClose(Core::IEditor *editor) {
    qWarning() << "QNVimPlugin::editorAboutToClose";
    if (!mBuffers.contains(editor))
        return;

    if (Core::EditorManager::currentEditor() == editor)
        mNumbersColumn->setEditor(nullptr);

    int bufferNumber = mBuffers[editor];
    mNVim->api2()->nvim_command(QStringLiteral("bd! %1").arg(mBuffers[editor]).toUtf8());
    mBuffers.remove(editor);
    mEditors.remove(bufferNumber);
    mChangedTicks.remove(bufferNumber);
    mBufferType.remove(bufferNumber);
}

void QNVimPlugin::initializeBuffer(int buffer) {
    QString bufferType = mBufferType[buffer];
    if (bufferType == "acwrite" or bufferType.isEmpty()) {
        connect(
            mNVim->api2()->nvim_buf_set_option(buffer, "undolevels", -1),
            &NeovimQt::MsgpackRequest::finished, this, [=]() {
                syncToVim(mEditors[buffer], [=]() {
                    mNVim->api2()->nvim_buf_set_option(buffer, "undolevels", -123456);
                    mNVim->api2()->nvim_buf_set_option(buffer, "modified", false);
                    if (bufferType.isEmpty() && QFile::exists(filename(mEditors[buffer])))
                        mNVim->api2()->nvim_buf_set_option(buffer, "buftype", "acwrite");
                });
            },
            Qt::DirectConnection);
    } else {
        mNVim->api2()->nvim_buf_set_option(buffer, "modified", false);
        syncFromVim();
    }
}

void QNVimPlugin::handleNotification(const QByteArray &name, const QVariantList &args) {
    auto editor = Core::EditorManager::currentEditor();

    if (!editor or !mBuffers.contains(editor))
        return;

    if (name == "Gui") {
        QByteArray method = args.first().toByteArray();
        QVariantList methodArgs = args.mid(1);
        if (method == "triggerCommand") {
            for (const auto& methodArg : methodArgs)
                triggerCommand(methodArg.toByteArray());
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
                    initializeBuffer(buffer);
                } else {
                    if (cmd == "TermOpen")
                        mNVim->api2()->nvim_command("doautocmd BufEnter");
                }
            } else if (cmd == "BufWriteCmd") {
                if (mEditors.contains(buffer)) {
                    QString currentFilename = this->filename(mEditors[buffer]);
                    if (mEditors[buffer]->document()->save(nullptr, Utils::FilePath::fromString(filename))) {
                        if (currentFilename != filename) {
                            mEditors.remove(buffer);
                            mChangedTicks.remove(buffer);
                            mBuffers.remove(editor);

                            auto request = mNVim->api2()->nvim_buf_set_name(buffer, filename.toUtf8());
                            connect(request, &NeovimQt::MsgpackRequest::finished, this, [=](quint32, quint64, const QVariant &) {
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
                                    e = Core::EditorManager::openEditor(filename);
                                else
                                    e = Core::EditorManager::openEditor(filename, "Core.PlainTextEditor");
                            } else {
                                e = Core::EditorManager::openEditor(filename);
                            }
                        } else {
                            qWarning() << 123;
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
                if (bufferListed and mEditors.contains(buffer) and mEditors[buffer]) {
                    if (Core::EditorManager::currentEditor() == mEditors[buffer])
                        mSettingBufferFromVim = -1;
                    Core::EditorManager::closeEditors({mEditors[buffer]});
                }
            } else if (cmd == "BufHidden") {
                if (
                    (bufferHidden == "wipe" or bufferHidden == "delete" or bufferHidden == "unload" or bufferType == "help") and
                    mEditors.contains(buffer) and mEditors[buffer]) {
                    if (Core::EditorManager::currentEditor() == mEditors[buffer])
                        mSettingBufferFromVim = -1;
                    Core::EditorManager::closeEditors({mEditors[buffer]});
                }
            } else if (cmd == "BufWipeout") {
                if (!bufferListed and mEditors.contains(buffer) and mEditors[buffer]) {
                    if (Core::EditorManager::currentEditor() == mEditors[buffer])
                        mSettingBufferFromVim = -1;
                    Core::EditorManager::closeEditors({mEditors[buffer]});
                }
            }
        }
    } else if (name == "redraw")
        redraw(args);
}

void QNVimPlugin::redraw(const QVariantList &args) {
    auto editor = Core::EditorManager::currentEditor();
    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
    bool shouldSync = false;
    bool flush = false;

    for (const auto& arg : args) {
        QVariantList line = arg.toList();
        QByteArray command = line.first().toByteArray();
        QVariantList args = line.mid(1).constFirst().toList();

        if (!command.startsWith("msg") and
            !command.startsWith("cmdline") and command != "flush")
            shouldSync = true;

        if (command == "flush")
            flush = true;

        if (command == "win_pos") {
            qWarning() << line;
        } else if (command == "win_float_pos") {
            qWarning() << line;
        } else if (command == "win_hide") {
            qWarning() << line;
        } else if (command == "win_close") {
            qWarning() << line;
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
        } else if (command == "grid_resize") {
            if (line.first().toInt() == 1) {
                mWidth = args[0].toInt();
                mHeight = args[1].toInt();
            }
        } else if (command == "default_colors_set") {
            qint64 val = args[0].toLongLong();
            if (val != -1) {
                mForegroundColor = QRgb(val);
                QPalette palette = textEditor->palette();
                palette.setColor(QPalette::WindowText, mForegroundColor);
                textEditor->setPalette(palette);
            }

            val = args[1].toLongLong();
            if (val != -1) {
                mBackgroundColor = QRgb(val);
                QPalette palette = textEditor->palette();
                palette.setBrush(QPalette::Window, mBackgroundColor);
                textEditor->setPalette(palette);
            }

            val = args[2].toLongLong();
            if (val != -1) {
                mSpecialColor = QRgb(val);
            }
        } else if (command == "cmdline_show") {
            mCMDLineVisible = true;
            QVariantList contentList = args[0].toList();
            mCMDLineContent.clear();

            for (const auto& contentItem : contentList)
                mCMDLineContent += QString::fromUtf8(contentItem.toList()[1].toByteArray());

            mCMDLinePos = args[1].toInt();
            mCMDLineFirstc = args[2].toString()[0];
            mCMDLinePrompt = QString::fromUtf8(args[3].toByteArray());
            mCMDLineIndent = args[4].toInt();
        } else if (command == "cmdline_pos") {
            mCMDLinePos = args[0].toInt();
        } else if (command == "cmdline_hide") {
            mCMDLineVisible = false;
        } else if (command == "msg_show") {
            QVariantList contentList = args[1].toList();
            mMessageLineDisplay.clear();
            for (const auto& contentItem : contentList)
                mMessageLineDisplay += QString::fromUtf8(contentItem.toList()[1].toByteArray());
        } else if (command == "msg_clear") {
            mMessageLineDisplay.clear();
        } else if (command == "msg_history_show") {
            QVariantList entries = args[1].toList();
            mMessageLineDisplay.clear();

            for (const auto& entry : entries) {
                QVariantList contentList = entry.toList()[1].toList();

                for (const auto& contentItem : contentList)
                    mMessageLineDisplay += QString::fromUtf8(contentItem.toList()[1].toByteArray()) + '\n';
            }
        }
    }

    if (shouldSync and flush)
        syncFromVim();

    updateCursorSize();

    QFontMetrics commandLineFontMetric(mCMDLine->font());
    if (mCMDLineVisible) {
        QString text = mCMDLineFirstc + mCMDLinePrompt + QString(mCMDLineIndent, ' ') + mCMDLineContent;

        if (mCMDLine->toPlainText() != text)
            mCMDLine->setPlainText(text);

        static const auto endLineRegExp = QRegularExpression("[\n\r]");

        const auto height = (text.count(endLineRegExp) + 1) * commandLineFontMetric.height();
        auto width = 0;

        const auto lines = text.split(endLineRegExp);
        for (const auto& line : lines) {
            width += commandLineFontMetric.horizontalAdvance(line);
        }

        if (mCMDLine->minimumWidth() != qMax(200, qMin(width + 10, 400)))
            mCMDLine->setMinimumWidth(qMax(200, qMin(width + 10, 400)));

        if (mCMDLine->minimumHeight() != qMax(25, qMin(height + 4, 400))) {
            mCMDLine->setMinimumHeight(qMax(25, qMin(height + 4, 400)));
            mCMDLine->parentWidget()->setFixedHeight(qMax(25, qMin(height + 4, 400)));
            mCMDLine->parentWidget()->parentWidget()->setFixedHeight(qMax(25, qMin(height + 4, 400)));
            mCMDLine->parentWidget()->parentWidget()->parentWidget()->setFixedHeight(qMax(25, qMin(height + 4, 400)));
        }

        if (!mCMDLine->hasFocus())
            mCMDLine->setFocus();

        QTextCursor cursor = mCMDLine->textCursor();
        if (cursor.position() != (QString(mCMDLineFirstc + mCMDLinePrompt).length() + mCMDLineIndent + mCMDLinePos)) {
            cursor.setPosition(QString(mCMDLineFirstc + mCMDLinePrompt).length() + mCMDLineIndent + mCMDLinePos);
            mCMDLine->setTextCursor(cursor);
        }

        if (mUIMode == "cmdline_normal") {
            if (mCMDLine->cursorWidth() != 1)
                mCMDLine->setCursorWidth(1);
        } else if (mUIMode == "cmdline_insert") {
            if (mCMDLine->cursorWidth() != 11)
                mCMDLine->setCursorWidth(11);
        }
    } else {
        mCMDLine->setPlainText(mMessageLineDisplay);

        if (mCMDLine->hasFocus())
            textEditor->setFocus();

        auto height = commandLineFontMetric.height();
        mCMDLine->setMinimumHeight(qMax(25, qMin(height + 4, 400)));
        mCMDLine->parentWidget()->setFixedHeight(qMax(25, qMin(height + 4, 400)));
        mCMDLine->parentWidget()->parentWidget()->setFixedHeight(qMax(25, qMin(height + 4, 400)));
        mCMDLine->parentWidget()->parentWidget()->parentWidget()->setFixedHeight(qMax(25, qMin(height + 4, 400)));
    }

    mCMDLine->setToolTip(mCMDLine->toPlainText());
}

void QNVimPlugin::updateCursorSize() {
    auto editor = Core::EditorManager::currentEditor();
    auto textEditor = qobject_cast<TextEditor::TextEditorWidget *>(editor->widget());
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

HelpEditorFactory::HelpEditorFactory() : PlainTextEditorFactory() {
    setId("Help");
    setDisplayName("Help");
    addMimeType("text/plain");
}

TerminalEditorFactory::TerminalEditorFactory() : PlainTextEditorFactory() {
    setId("Terminal");
    setDisplayName("Terminal");
    addMimeType("text/plain");
}

} // namespace Internal
} // namespace QNVim
