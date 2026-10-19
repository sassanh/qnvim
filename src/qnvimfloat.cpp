#include "qnvimfloat.h"

#include <QFontMetricsF>
#include <QPainter>

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

} // namespace Internal
} // namespace QNVim
