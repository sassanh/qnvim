/****************************************************************************
**
** Copyright (C) 2018-2019 Sassan Haradji
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** MIT License
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
****************************************************************************/

#pragma once

#include "qnvim_global.h"
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
    class InputConv;
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

private slots:
    // Save cursor flash time to variable instead of changing real value
    void saveCursorFlashTime(int cursorFlashTime);

private:
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
    NeovimQt::InputConv *mInputConv;
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

    int mSavedCursorFlashTime = -1;
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
