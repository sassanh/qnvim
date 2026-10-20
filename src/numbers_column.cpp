#include "numbers_column.h"

#include <texteditor/fontsettings.h>
#include <texteditor/textdocument.h>
#include <texteditor/textdocumentlayout.h>
#include <texteditor/texteditor.h>
#include <texteditor/texteditorsettings.h>

#include <utils/plaintextedit/plaintextedit.h>

#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>

namespace QNVim {
namespace Internal {

NumbersColumn::NumbersColumn() {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    connect(TextEditor::TextEditorSettings::instance(),
            &TextEditor::TextEditorSettings::fontSettingsChanged,
            this, &NumbersColumn::updateGeometry);
    connect(TextEditor::TextEditorSettings::instance(),
            &TextEditor::TextEditorSettings::behaviorSettingsChanged,
            this, &NumbersColumn::updateGeometry);
}

void NumbersColumn::setEditor(TextEditor::TextEditorWidget *editor) {
    if (editor == mEditor)
        return;

    if (mEditor) {
        mEditor->removeEventFilter(this);
        disconnect(mEditor, &Utils::PlainTextEdit::cursorPositionChanged,
                   this, &NumbersColumn::updateGeometry);
        disconnect(mEditor->verticalScrollBar(), &QScrollBar::valueChanged,
                   this, &NumbersColumn::updateGeometry);
        disconnect(mEditor->document(), &QTextDocument::contentsChanged,
                   this, &NumbersColumn::updateGeometry);
        disconnect(mEditor, &Utils::PlainTextEdit::blockCountChanged,
                   this, &NumbersColumn::updateGeometry);
        disconnect(mEditor->textDocument(), &TextEditor::TextDocument::fontSettingsChanged,
                   this, &NumbersColumn::updateGeometry);
        if (auto layout = qobject_cast<TextEditor::TextDocumentLayout *>(
                mEditor->document()->documentLayout())) {
            disconnect(layout, &TextEditor::TextDocumentLayout::updateExtraArea,
                       this, &NumbersColumn::updateGeometry);
        }
    }

    mEditor = editor;
    setParent(mEditor);

    if (mEditor) {
        mEditor->installEventFilter(this);
        connect(mEditor, &Utils::PlainTextEdit::cursorPositionChanged,
                this, &NumbersColumn::updateGeometry);
        connect(mEditor->verticalScrollBar(), &QScrollBar::valueChanged,
                this, &NumbersColumn::updateGeometry);
        connect(mEditor->document(), &QTextDocument::contentsChanged,
                this, &NumbersColumn::updateGeometry);
        connect(mEditor, &Utils::PlainTextEdit::blockCountChanged,
                this, &NumbersColumn::updateGeometry);
        connect(mEditor->textDocument(), &TextEditor::TextDocument::fontSettingsChanged,
                this, &NumbersColumn::updateGeometry);
        if (auto layout = qobject_cast<TextEditor::TextDocumentLayout *>(
                mEditor->document()->documentLayout())) {
            connect(layout, &TextEditor::TextDocumentLayout::updateExtraArea,
                    this, &NumbersColumn::updateGeometry);
        }
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

    const int currentLine = mEditor->textCursor().block().blockNumber();

    QPainter p(this);
    QPalette pal = mEditor->extraArea()->palette();
    pal.setCurrentColorGroup(QPalette::Active);
    const QColor fg = pal.color(QPalette::Dark);
    const QColor bg = pal.color(QPalette::Window);
    const bool coverNativeNumbers = mEditor->lineNumbersVisible();
    const int fallbackLineHeight = mEditor->extraArea()->fontMetrics().lineSpacing();
    const int lastVisibleLine = mEditor->lastVisibleBlockNumber();

    // Ask the editor for each line's cursor rect, as TextEditorWidget does when
    // it paints its own line numbers: the rect is on the first visual row of
    // the block, so wrapped lines, folds and the partially visible top line
    // stay aligned with the text.
    for (int lineNumber = mEditor->firstVisibleBlockNumber(); lineNumber <= lastVisibleLine;
         ++lineNumber) {
        const QTextBlock block = mEditor->document()->findBlockByNumber(lineNumber);
        if (not block.isValid())
            break;
        if (not block.isVisible())
            continue;

        const QRect cursorRect = mEditor->cursorRect(QTextCursor(block));
        const int lineHeight = cursorRect.height() > 0 ? cursorRect.height() : fallbackLineHeight;
        const QRectF rect(0, cursorRect.top(), width(), lineHeight);

        if (rect.bottom() < 0 or not rect.intersects(event->rect()))
            continue;

        const int relativeLine = block.blockNumber() - currentLine;
        const bool current = relativeLine == 0;
        const QString number = current
            ? QString::number(mNumber ? block.blockNumber() + 1 : 0)
            : QString::number(qAbs(relativeLine));

        if (coverNativeNumbers)
            p.fillRect(rect, bg);

        p.setPen(fg);
        p.setFont(font());
        p.drawText(rect, Qt::AlignRight | Qt::AlignVCenter, number);
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

    const QFont font = mEditor->extraArea()->font();
    setFont(font);

    const QRect extraArea = mEditor->extraArea()->geometry();
    int markWidth = 0;
    const int extraAreaWidth = mEditor->extraAreaWidth(&markWidth);

    int foldWidth = 0;
    if (mEditor->codeFoldingVisible()) {
        const int lineSpacing = mEditor->extraArea()->fontMetrics().lineSpacing();
        foldWidth = lineSpacing + lineSpacing % 2 + 1;
    }

    // The line-number column starts after the marks and ends before the folding
    // column and the four pixels of padding TextEditorWidget reserves on its
    // right (see TextEditorWidget::extraAreaWidth()).
    QRect rect = extraArea;
    rect.setLeft(extraArea.left() + markWidth);
    rect.setRight(extraArea.left() + extraAreaWidth - 4 - foldWidth);
    setGeometry(rect);

    update();
}

} // namespace Internal
} // namespace QNVim
