#pragma once

#include <QColor>
#include <QFont>
#include <QString>
#include <QVector>
#include <QWidget>

namespace QNVim {
namespace Internal {

// A cell of the grid of a floating window, as far as drawing it is concerned:
// its colors and style are already resolved (Neovim's default colors, reverse
// video and blending are applied), so the cell can be drawn as it is.
struct FloatCell {
    QString text;
    QColor foreground;
    QColor background;
    bool bold = false;
    bool italic = false;
    bool underline = false;
};

// Shows one of Neovim's floating windows (a picker, a hover popup, ...): a
// frameless, border-less overlay inside Qt Creator's main window that draws
// the cells Neovim reports for the float's grid, with the editor's font. The
// overlay is only a view: it never takes focus and lets the mouse through to
// the editor below, so keys and clicks keep reaching Neovim through it.
class FloatWidget : public QWidget {
  public:
    explicit FloatWidget(QWidget *parent = nullptr);

    // The font the cells are drawn with: the font of the editor below.
    void setGridFont(const QFont &font);
    // The colors of cells Neovim did not draw (yet).
    void setDefaultColors(const QColor &foreground, const QColor &background);

    // The grid, as Neovim reports it (grid_resize, grid_clear, grid_line,
    // grid_scroll).
    void resizeGrid(int rows, int cols);
    void clearGrid();
    void setLine(int row, int col, const QVector<FloatCell> &cells);
    void scrollGrid(int top, int bot, int left, int right, int rows, int cols);
    int rows() const { return mRows; }
    int cols() const { return mCols; }

    // The size the grid needs in pixels, with the current font.
    QSize gridSize() const;

  protected:
    void paintEvent(QPaintEvent *) override;

  private:
    const FloatCell &cellAt(int row, int col) const;
    FloatCell blankCell() const;
    void updateCellMetrics();

    QVector<FloatCell> mCells;
    int mRows = 0;
    int mCols = 0;
    qreal mCellWidth = 1;
    qreal mCellHeight = 1;
    qreal mAscent = 0;
    QColor mDefaultForeground;
    QColor mDefaultBackground;
};

} // namespace Internal
} // namespace QNVim
