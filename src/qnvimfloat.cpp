#include "qnvimfloat.h"

#include <coreplugin/icore.h>
#include <texteditor/fontsettings.h>
#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>

#include <QFontMetricsF>
#include <QPainter>
#include <QStatusBar>
#include <QTextCursor>

#include <algorithm>

namespace QNVim {
namespace Internal {

FloatWidget::FloatWidget(QWidget *parent)
    : QWidget(parent) {
    // The overlay is only a view of Neovim's window: the keys typed while it is
    // on screen are handled by the editor below, and mouse events pass through
    // to it as well (as they did before, when the editor showed the area the
    // float covers).
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAutoFillBackground(false);
    updateCellMetrics();
}

void FloatWidget::setGridFont(const QFont &font) {
    if (this->font() == font)
        return;

    QWidget::setFont(font);
    updateCellMetrics();
    update();
}

void FloatWidget::updateCellMetrics() {
    const QFontMetricsF metrics(font());
    mCellWidth = metrics.horizontalAdvance(QLatin1Char('A'));
    mCellHeight = metrics.lineSpacing();
    mAscent = metrics.ascent();
    if (mCellWidth <= 0)
        mCellWidth = 1;
    if (mCellHeight <= 0)
        mCellHeight = 1;
}

void FloatWidget::setDefaultColors(const QColor &foreground, const QColor &background) {
    if (mDefaultForeground == foreground and mDefaultBackground == background)
        return;

    mDefaultForeground = foreground;
    mDefaultBackground = background;
}

FloatCell FloatWidget::blankCell() const {
    FloatCell cell;
    cell.foreground = mDefaultForeground;
    cell.background = mDefaultBackground;
    return cell;
}

void FloatWidget::resizeGrid(int rows, int cols) {
    rows = qMax(0, rows);
    cols = qMax(0, cols);
    if (rows == mRows and cols == mCols)
        return;

    QVector<FloatCell> cells(rows * cols, blankCell());
    // Keep what both grids have in common; Neovim repaints the rest.
    for (int row = 0; row < qMin(rows, mRows); ++row) {
        for (int col = 0; col < qMin(cols, mCols); ++col)
            cells[row * cols + col] = mCells[row * mCols + col];
    }

    mCells = cells;
    mRows = rows;
    mCols = cols;
    update();
}

void FloatWidget::clearGrid() {
    mCells.fill(blankCell());
    update();
}

void FloatWidget::setLine(int row, int col, const QVector<FloatCell> &cells) {
    if (row < 0 or row >= mRows or col < 0)
        return;

    for (int i = 0; i < cells.size() and col + i < mCols; ++i)
        mCells[row * mCols + col + i] = cells[i];

    update();
}

void FloatWidget::scrollGrid(int top, int bot, int left, int right, int rows, int cols) {
    top = qMax(0, top);
    bot = qMin(bot, mRows);
    left = qMax(0, left);
    right = qMin(right, mCols);
    if (top >= bot or left >= right)
        return;

    // Copy the cells the way Neovim's grid_scroll says: a positive `rows` moves
    // the region up, a negative one moves it down; `cols` does the same
    // horizontally. The source and destination regions overlap, so copy in an
    // order that does not overwrite cells that still have to be copied.
    if (rows > 0) {
        for (int row = top; row < bot - rows; ++row) {
            for (int col = left; col < right; ++col)
                mCells[row * mCols + col] = mCells[(row + rows) * mCols + col];
        }
    } else if (rows < 0) {
        for (int row = bot - 1; row >= top - rows; --row) {
            for (int col = left; col < right; ++col)
                mCells[row * mCols + col] = mCells[(row + rows) * mCols + col];
        }
    }

    if (cols > 0) {
        for (int row = top; row < bot; ++row) {
            for (int col = left; col < right - cols; ++col)
                mCells[row * mCols + col] = mCells[row * mCols + col + cols];
        }
    } else if (cols < 0) {
        for (int row = top; row < bot; ++row) {
            for (int col = right - 1; col >= left - cols; --col)
                mCells[row * mCols + col] = mCells[row * mCols + col + cols];
        }
    }

    update();
}

QSize FloatWidget::gridSize() const {
    return QSize(qCeil(mCols * mCellWidth), qCeil(mRows * mCellHeight));
}

const FloatCell &FloatWidget::cellAt(int row, int col) const {
    return mCells.at(row * mCols + col);
}

void FloatWidget::paintEvent(QPaintEvent *) {
    if (mRows <= 0 or mCols <= 0)
        return;

    QPainter painter(this);
    painter.setFont(font());

    // The backgrounds first, in runs of cells of one color (usually a whole
    // row), so that the float is solid where it has no text.
    for (int row = 0; row < mRows; ++row) {
        const qreal y = row * mCellHeight;
        for (int col = 0; col < mCols;) {
            const QColor background = cellAt(row, col).background;
            int end = col + 1;
            while (end < mCols and cellAt(row, end).background == background)
                ++end;

            painter.fillRect(QRectF(col * mCellWidth, y, (end - col) * mCellWidth, mCellHeight),
                             background);
            col = end;
        }
    }

    // Then the text, in runs of neighbouring cells that share their style: a
    // font and a pen per cell would be much slower, and a picker repaints on
    // every key press. A cell whose text does not fit in one cell (a double
    // width character) ends a run.
    for (int row = 0; row < mRows; ++row) {
        const qreal y = row * mCellHeight;
        for (int col = 0; col < mCols;) {
            const FloatCell &first = cellAt(row, col);
            if (first.text.isEmpty()) {
                ++col;
                continue;
            }

            QString text = first.text;
            int end = col + 1;
            if (first.text.size() == 1) {
                while (end < mCols) {
                    const FloatCell &next = cellAt(row, end);
                    if (next.text.size() != 1 or next.foreground != first.foreground
                        or next.bold != first.bold or next.italic != first.italic
                        or next.underline != first.underline)
                        break;

                    text += next.text;
                    ++end;
                }
            }

            QFont runFont = font();
            runFont.setBold(first.bold);
            runFont.setItalic(first.italic);
            painter.setFont(runFont);
            painter.setPen(first.foreground);
            painter.drawText(QPointF(col * mCellWidth, y + mAscent), text);

            if (first.underline) {
                const qreal lineY = y + mAscent + 1;
                painter.drawLine(QPointF(col * mCellWidth, lineY),
                                 QPointF(end * mCellWidth, lineY));
            }

            col = end;
        }
    }
}

void FloatLayer::setDefaultColors(const QColor &foreground, const QColor &background) {
    if (mDefaultForeground == foreground and mDefaultBackground == background)
        return;

    mDefaultForeground = foreground;
    mDefaultBackground = background;
}

void FloatLayer::setHighlight(int id, const QVariantMap &attributes) {
    HlAttr &hl = mHlAttrs[id];
    const int foreground = attributes.value("foreground", -1).toInt();
    const int background = attributes.value("background", -1).toInt();
    hl.foreground = foreground < 0 ? QColor() : QColor::fromRgb(QRgb(foreground));
    hl.background = background < 0 ? QColor() : QColor::fromRgb(QRgb(background));
    hl.blend = attributes.value("blend", 0).toInt();
    hl.reverse = attributes.value("reverse", false).toBool();
    hl.bold = attributes.value("bold", false).toBool();
    hl.italic = attributes.value("italic", false).toBool();
    hl.underline = attributes.value("underline", false).toBool();
}

void FloatLayer::floatPositioned(int grid, int anchorGrid, int compindex, int row, int col) {
    FloatState &st = mFloats[grid];
    st.isFloat = true;
    st.visible = true;
    st.anchorGrid = anchorGrid;
    st.compindex = compindex;
    st.row = row;
    st.col = col;
}

void FloatLayer::hideFloat(int grid) {
    auto floatState = mFloats.find(grid);
    if (floatState == mFloats.end() or !floatState->isFloat)
        return;

    floatState->visible = false;
    if (floatState->widget)
        floatState->widget->hide();
}

void FloatLayer::closeFloat(int grid) {
    auto floatState = mFloats.find(grid);
    if (floatState == mFloats.end())
        return;

    delete floatState->widget.data();
    mFloats.erase(floatState);
}

void FloatLayer::destroyGrid(int grid) {
    mGridSizes.remove(grid);
    closeFloat(grid);
}

void FloatLayer::forgetGrid(int grid) {
    auto floatState = mFloats.find(grid);
    if (floatState != mFloats.end() and !floatState->isFloat)
        closeFloat(grid);
}

void FloatLayer::clearFloats() {
    for (auto &floatState : mFloats) {
        delete floatState.widget.data();
        floatState.widget = nullptr;
    }
    mFloats.clear();
}

FloatLayer::FloatState *FloatLayer::rememberGrid(int grid) {
    // The screen (grid 1) is drawn for the whole main window; it is not a
    // float.
    if (grid == 1)
        return nullptr;

    return &mFloats[grid];
}

FloatWidget *FloatLayer::floatWidget(int grid) {
    auto floatState = mFloats.find(grid);
    if (floatState == mFloats.end())
        return nullptr;

    if (!floatState->widget) {
        auto mainWindow = Core::ICore::mainWindow();
        if (!mainWindow)
            return nullptr;

        floatState->widget = new FloatWidget(mainWindow);
        if (const QSize size = mGridSizes.value(grid); size.isValid())
            floatState->widget->resizeGrid(size.height(), size.width());
        floatState->widget->hide(); // place() shows it
    }

    return floatState->widget;
}

void FloatLayer::setFloatGridSize(int grid, int rows, int cols) {
    if (!rememberGrid(grid))
        return;

    mGridSizes.insert(grid, QSize(cols, rows));
    if (FloatWidget *widget = floatWidget(grid))
        widget->resizeGrid(rows, cols);
}

void FloatLayer::setFloatGridLine(int grid, int row, int col, const QVariantList &cells) {
    if (!rememberGrid(grid))
        return;

    FloatWidget *widget = floatWidget(grid);
    if (!widget)
        return;

    QVector<FloatCell> line;
    int hlId = 0;
    for (const QVariant &cellValue : cells) {
        const QVariantList cell = cellValue.toList();
        if (cell.isEmpty())
            continue;

        const QString text = QString::fromUtf8(cell[0].toByteArray());
        if (cell.size() >= 2)
            hlId = cell[1].toInt();
        const int repeat = cell.size() >= 3 ? cell[2].toInt() : 1;

        // A cell can hold more than one character, and the right half of a
        // double width character is a cell with an empty text.
        for (int r = 0; r < repeat; ++r) {
            for (int i = 0; i < text.size(); ++i) {
                QString character = text.at(i);
                if (character.at(0).isHighSurrogate() and i + 1 < text.size())
                    character += text.at(++i);
                line.append(floatCell(hlId, character));
            }
        }
    }

    widget->setLine(row, col, line);
}

void FloatLayer::scrollFloatGrid(int grid, int top, int bot, int left, int right, int rows,
                                 int cols) {
    if (!rememberGrid(grid))
        return;

    if (FloatWidget *widget = floatWidget(grid))
        widget->scrollGrid(top, bot, left, right, rows, cols);
}

void FloatLayer::clearFloatGrid(int grid) {
    if (!rememberGrid(grid))
        return;

    if (FloatWidget *widget = floatWidget(grid))
        widget->clearGrid();
}

// The cell a grid_line cell is drawn as: the colors and style come from the
// cell's highlight and Neovim's defaults.
FloatCell FloatLayer::floatCell(int hlId, const QString &text) const {
    auto attribute = mHlAttrs.constFind(hlId);
    const HlAttr hl = attribute == mHlAttrs.constEnd() ? HlAttr() : attribute.value();

    QColor foreground = hl.foreground.isValid() ? hl.foreground : mDefaultForeground;
    QColor background = hl.background.isValid() ? hl.background : mDefaultBackground;
    if (hl.reverse)
        qSwap(foreground, background);
    if (hl.blend > 0)
        background.setAlpha(255 * (100 - hl.blend) / 100);

    FloatCell cell;
    cell.text = text;
    cell.foreground = foreground;
    cell.background = background;
    cell.bold = hl.bold;
    cell.italic = hl.italic;
    cell.underline = hl.underline;
    return cell;
}

// Where the editor draws the first character of its first visible row, in
// global coordinates. The extra area (line numbers, marks, folding markers) is
// outside the viewport, so the cursor rects are the way to ask.
static QPoint editorTextOrigin(const TextEditor::TextEditorWidget *textEditor) {
    const QTextCursor firstVisibleCursor = textEditor->cursorForPosition(QPoint(0, 0));
    QTextCursor firstColumnCursor = firstVisibleCursor;
    firstColumnCursor.movePosition(QTextCursor::StartOfBlock);
    const QPoint viewport = textEditor->viewport()->mapToGlobal(QPoint(0, 0));
    return viewport + QPoint(textEditor->cursorRect(firstColumnCursor).x(),
                             textEditor->cursorRect(firstVisibleCursor).y());
}

// Neovim reports every float's position in cells of its screen (win_float_pos'
// screen_row/screen_col) and the grid it is anchored to (anchor_grid). qnvim
// sizes the screen (grid 1) to the whole main window, so a float anchored to
// the screen is shown at that cell of the main window. A float anchored to a
// window is placed through the editor view of that window instead: the
// window's cell on Neovim's screen is not where Qt Creator shows its text,
// since the views can be anywhere in the main window.
void FloatLayer::place(const QList<WindowAnchor> &windowAnchors,
                       TextEditor::TextEditorWidget *currentEditor) {
    if (mFloats.isEmpty())
        return;

    auto mainWindow = Core::ICore::mainWindow();
    if (!mainWindow or !currentEditor) {
        for (auto &floatState : mFloats) {
            if (floatState.widget)
                floatState.widget->hide();
        }
        return;
    }

    const QFont font = currentEditor->textDocument()->fontSettings().font();
    const QFontMetricsF metrics(font);
    const qreal cellWidth = metrics.horizontalAdvance(QLatin1Char('A'));
    const qreal cellHeight = metrics.lineSpacing();
    if (cellWidth <= 0 or cellHeight <= 0)
        return;

    const QPoint mainWindowOrigin = mainWindow->mapToGlobal(QPoint(0, 0));

    // Qt Creator's status bar is below the editor area, but it can be an
    // overlay on top of it: never put a float placed in a view under it.
    // Ask Core for the status bar: Qt Creator puts its own into the main
    // window's layout instead of setting it with QMainWindow::setStatusBar(),
    // so QMainWindow::statusBar() would create an empty status bar there and
    // leave a strip at the bottom of the window for the rest of the session.
    int statusBarTop = mainWindow->height();
    if (QStatusBar *statusBar = Core::ICore::statusBar();
        statusBar and statusBar->isVisible())
        statusBarTop = statusBar->mapTo(mainWindow, QPoint(0, 0)).y();

    // Neovim's compindex is the order the floats are stacked in: place them in
    // it, so that raising each one leaves the last (topmost) one on top.
    QList<int> grids = mFloats.keys();
    std::sort(grids.begin(), grids.end(), [this](int first, int second) {
        return mFloats.value(first).compindex < mFloats.value(second).compindex;
    });

    for (const int grid : grids) {
        auto floatState = mFloats.find(grid);
        if (floatState == mFloats.end() or !floatState->isFloat)
            continue;

        FloatWidget *widget = floatWidget(grid);
        if (!widget)
            continue;

        widget->setGridFont(font);
        widget->setDefaultColors(mDefaultForeground, mDefaultBackground);

        if (!floatState->visible or widget->rows() <= 0 or widget->cols() <= 0) {
            widget->hide();
            continue;
        }

        const QSize gridSize = widget->gridSize();

        // Neovim anchors a float to the screen (grid 1) or to a window's grid
        // (win_float_pos' anchor_grid). A float anchored to a window is placed
        // through the editor view that shows that window: Neovim's window
        // layout is not Qt Creator's editor layout (the views can be anywhere
        // in the main window), so the window's cell on Neovim's screen is not
        // where its text is shown. Every other float stays at its screen cell,
        // which is a cell of the whole main window.
        const WindowAnchor *windowAnchor = nullptr;
        if (floatState->anchorGrid != 1) {
            for (const WindowAnchor &anchor : windowAnchors) {
                if (anchor.grid == floatState->anchorGrid and anchor.editor) {
                    windowAnchor = &anchor;
                    break;
                }
            }
        }

        bool placedInEditor = false;
        if (windowAnchor) {
            auto targetEditor = windowAnchor->textEditor;
            if (targetEditor) {
                // The window's grid starts textoff columns and winbar rows
                // before the first character of the text. Move the float from
                // the window's screen cell to the editor view's text area,
                // then add its offset from the window's top left corner.
                const QPoint gridOrigin
                    = editorTextOrigin(targetEditor) - mainWindowOrigin
                      - QPoint(qRound(windowAnchor->textoff * cellWidth),
                               qRound(windowAnchor->winbar * cellHeight));
                QPoint origin
                    = gridOrigin
                      + QPoint(qRound((floatState->col - windowAnchor->startCol) * cellWidth),
                               qRound((floatState->row - windowAnchor->startRow) * cellHeight));

                // The editor view's text ends above qnvim's command line
                // widget (and above Qt Creator's status bar if it overlays
                // the view): keep the whole float there, so a completion menu
                // does not cover the command line.
                const int textBottom
                    = targetEditor->mapTo(mainWindow, QPoint(0, targetEditor->height())).y();
                const int bottom = qMin(textBottom, statusBarTop);
                origin.setY(qMin(origin.y(), bottom - gridSize.height()));

                // Keep the float inside the main window horizontally; the
                // vertical position is already clamped above.
                origin.setX(qBound(0, origin.x(),
                                   qMax(0, mainWindow->width() - gridSize.width())));
                origin.setY(qBound(0, origin.y(), qMax(0, bottom - gridSize.height())));

                widget->setGeometry(QRect(origin, gridSize));
                widget->show();
                widget->raise();
                placedInEditor = true;
            }
        }

        if (placedInEditor)
            continue;

        // A float anchored to another float (a completion menu's scrollbar or
        // documentation window) is an offset from that float's overlay: follow
        // it, since the anchor's own cell has no relation to the overlay.
        if (floatState->anchorGrid != 1) {
            auto anchor = mFloats.find(floatState->anchorGrid);
            if (anchor != mFloats.end() and anchor->widget) {
                QPoint origin
                    = anchor->widget->geometry().topLeft()
                      + QPoint(qRound((floatState->col - anchor->col) * cellWidth),
                               qRound((floatState->row - anchor->row) * cellHeight));
                origin.setX(qBound(0, origin.x(),
                                   qMax(0, mainWindow->width() - gridSize.width())));
                origin.setY(qBound(0, origin.y(), qMax(0, statusBarTop - gridSize.height())));
                widget->setGeometry(QRect(origin, gridSize));
                widget->show();
                widget->raise();
                continue;
            }
        }

        const QPoint origin = QPointF(floatState->col * cellWidth,
                                      floatState->row * cellHeight).toPoint();
        widget->setGeometry(QRect(origin, gridSize));
        widget->show();
        widget->raise();
    }
}

} // namespace Internal
} // namespace QNVim
