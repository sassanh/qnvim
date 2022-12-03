// SPDX-FileCopyrightText: 2018-2019 Sassan Haradji <sassanh@gmail.com>
// SPDX-License-Identifier: MIT

#include "numbers_column.h"

#include <texteditor/fontsettings.h>
#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>
#include <texteditor/texteditorsettings.h>

#include <QPainter>
#include <QScrollBar>

namespace QNVim {
namespace Internal {

NumbersColumn::NumbersColumn() {
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
    } else
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
    const QColor fg = pal.color(QPalette::WindowText);
    const QColor bg = pal.color(QPalette::Window);
    p.setPen(fg);

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
