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

#include "numbers_column.h"

#include <texteditor/fontsettings.h>
#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>
#include <texteditor/texteditorsettings.h>

#include <QScrollBar>
#include <QPainter>

namespace QNVim {
namespace Internal {

NumbersColumn::NumbersColumn()
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    connect(TextEditor::TextEditorSettings::instance(),
        &TextEditor::TextEditorSettings::displaySettingsChanged,
        this, &NumbersColumn::updateGeometry);
}

void NumbersColumn::setEditor(TextEditor::TextEditorWidget *editor) {
    if (editor == mEditor)
        return;
    if (mEditor) {
        mEditor->removeEventFilter(this);
        disconnect(mEditor, &QPlainTextEdit::cursorPositionChanged,
                this, &NumbersColumn::updateGeometry);
        disconnect(mEditor->verticalScrollBar(), &QScrollBar::valueChanged,
                this, &NumbersColumn::updateGeometry);
        disconnect(mEditor->document(), &QTextDocument::contentsChanged,
                this, &NumbersColumn::updateGeometry);
    }
    mEditor = editor;
    setParent(mEditor);
    if (mEditor) {
        mEditor->installEventFilter(this);
        connect(mEditor, &QPlainTextEdit::cursorPositionChanged,
                this, &NumbersColumn::updateGeometry);
        connect(mEditor->verticalScrollBar(), &QScrollBar::valueChanged,
                this, &NumbersColumn::updateGeometry);
        connect(mEditor->document(), &QTextDocument::contentsChanged,
                this, &NumbersColumn::updateGeometry);
        show();
    }
    else
        hide();
    updateGeometry();
}

void NumbersColumn::setNumber(bool number) {
    mNumber = number;
    updateGeometry();
}

void NumbersColumn::paintEvent(QPaintEvent *event) {
    if (not mEditor)
        return;
    QTextCursor firstVisibleCursor = mEditor->cursorForPosition(QPoint(0, 0));
    QTextBlock firstVisibleBlock = firstVisibleCursor.block();
    if (firstVisibleCursor.positionInBlock() > 0) {
        firstVisibleBlock = firstVisibleBlock.next();
        firstVisibleCursor.setPosition(firstVisibleBlock.position());
    }

    QTextBlock block = mEditor->textCursor().block();
    bool forward = firstVisibleBlock.blockNumber() > block.blockNumber();
    int n = 0;
    while (block.isValid() and block != firstVisibleBlock) {
        block = forward ? block.next() : block.previous();
        if (block.isVisible())
            n += forward ? 1 : -1;
    }

    QPainter p(this);
    QPalette pal = mEditor->extraArea()->palette();
    const QColor fg = pal.color(QPalette::Dark);
    const QColor bg = pal.color(QPalette::Background);
    p.setPen(fg);
    QFontMetricsF fm(mEditor->textDocument()->fontSettings().font());
    qreal lineHeight = block.layout()->boundingRect().height();
    QRectF rect(0, mEditor->cursorRect(firstVisibleCursor).y(), width(), lineHeight);
    bool hideLineNumbers = mEditor->lineNumbersVisible();
    while (block.isValid()) {
        if (block.isVisible()) {
            if ((not mNumber or n != 0) and rect.intersects(event->rect())) {
                const int line = qAbs(n);
                const QString number = QString::number(line);
                if (hideLineNumbers)
                    p.fillRect(rect, bg);
                if (hideLineNumbers or line < 100)
                    p.drawText(rect, Qt::AlignRight | Qt::AlignVCenter, number);
            }

            rect.translate(0, lineHeight * block.lineCount());
            if (rect.y() > height())
                break;

            ++n;
        }

        block = block.next();
    }
}

bool NumbersColumn::eventFilter(QObject *, QEvent *event) {
    if (event->type() == QEvent::Resize or event->type() == QEvent::Move)
        updateGeometry();
    return false;
}

void NumbersColumn::updateGeometry() {
    if (not mEditor)
        return;
    QFontMetrics fm(mEditor->textDocument()->fontSettings().font());
    int lineHeight = fm.lineSpacing();
    setFont(mEditor->extraArea()->font());

    QRect rect = mEditor->extraArea()->geometry().adjusted(0, 0, -3, 0);
    bool marksVisible = mEditor->marksVisible();
    bool lineNumbersVisible = mEditor->lineNumbersVisible();
    bool foldMarksVisible = mEditor->codeFoldingVisible();
    if (marksVisible and lineNumbersVisible)
        rect.setLeft(lineHeight);
    if (foldMarksVisible and (marksVisible or lineNumbersVisible))
        rect.setRight(rect.right() - (lineHeight + lineHeight % 2));
    setGeometry(rect);

    update();
}

} // namespace Internal
} // namespace QNVim
