#pragma once

#include <QColor>
#include <QFont>
#include <QMap>
#include <QPointer>
#include <QSize>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QWidget>

namespace Core {
class IEditor;
}

namespace TextEditor {
class TextEditorWidget;
}

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

// The floating windows of Neovim (a picker, a hover popup, ...), shown as
// overlays inside Qt Creator's main window (see FloatWidget). The layer keeps
// the state of the floats, the highlight definitions their cells are drawn
// with and the size every grid last reported, redraws the overlays from the
// events of the grids Neovim draws floats in, and places them once Neovim has
// flushed a redraw.
//
// The events of a grid can arrive before the event that says what the grid is
// (win_float_pos for a float, win_pos for a window): the content is remembered
// until then (rememberGrid()), and a grid that turns out to be a window's is
// dropped again (see forgetGrid()); the events of the windows' grids are not
// forwarded to the layer at all. The screen (grid 1) is not a grid the layer
// draws.
class FloatLayer {
  public:
    // Where a Neovim window sits on Neovim's screen and which editor view
    // shows it: what the layer needs to place a float anchored to the
    // window's grid. A snapshot of this is handed to place(); the core knows
    // the windows (see QNVimCore::windowAnchors()).
    struct WindowAnchor {
        // The window's grid.
        int grid = 0;
        // Where the window's grid starts on Neovim's screen, in cells.
        int startRow = 0;
        int startCol = 0;
        // The window's external columns (line numbers, sign and fold columns,
        // 'statuscolumn') and its winbar rows: the window's grid starts that
        // much before the first character of its text.
        int textoff = -1;
        int winbar = 0;
        // The editor the window shows, and its text widget (null for editors
        // that are not text editors).
        Core::IEditor *editor = nullptr;
        TextEditor::TextEditorWidget *textEditor = nullptr;
    };

    // Neovim's default colors (default_colors_set): the colors of the cells
    // whose highlight leaves them to the UI, and of cells not drawn yet.
    void setDefaultColors(const QColor &foreground, const QColor &background);

    // A highlight definition from Neovim (hl_attr_define).
    void setHighlight(int id, const QVariantMap &attributes);

    // The redraw events of the grids Neovim draws floats in (the core
    // forwards them; see QNVimCore::redrawEvent() for the events' shape).
    void floatPositioned(int grid, int anchorGrid, int compindex, int row, int col);
    void hideFloat(int grid);
    void closeFloat(int grid);
    void destroyGrid(int grid);
    void setFloatGridSize(int grid, int rows, int cols);
    void setFloatGridLine(int grid, int row, int col, const QVariantList &cells);
    void scrollFloatGrid(int grid, int top, int bot, int left, int right, int rows, int cols);
    void clearFloatGrid(int grid);

    // The grid turned out to be a Neovim window's (win_pos/win_viewport):
    // drop the state remembered for its events, unless it is a float.
    void forgetGrid(int grid);

    // Delete every overlay and forget every float (Neovim is gone, or qnvim
    // shuts down).
    void clearFloats();

    // Show the visible floats with a size, stacked in Neovim's order. A float
    // anchored to a window is placed in that window's editor view (see
    // WindowAnchor); one anchored to another float follows that float's
    // overlay; every other float is placed at its cell of the main window.
    // The cells are drawn with the font of the current editor (Neovim's
    // screen is its text area).
    void place(const QList<WindowAnchor> &windowAnchors,
               TextEditor::TextEditorWidget *currentEditor);

  private:
    // A floating window Neovim shows (a picker, a hover popup, ...): where it
    // is on Neovim's screen, how it stacks with the other floats and the
    // overlay that shows it (see FloatWidget). A grid whose events the layer
    // saw is remembered with this state as well, before it is known to be a
    // float (win_float_pos) or a window's grid (win_pos): `isFloat` says
    // which it turned out to be.
    struct FloatState {
        bool isFloat = false;
        // The grid the float is anchored to (win_float_pos' anchor_grid): the
        // screen (grid 1) or a window's grid. A float anchored to a window is
        // mapped through the editor view of that window (see place()).
        int anchorGrid = 1;
        // Top left corner of the float on Neovim's screen, in cells. The
        // screen is grid 1, which qnvim sizes to the whole main window, so
        // these are cells of the main window.
        int row = 0;
        int col = 0;
        // Neovim's rendering order of the floats: a float with a higher
        // compindex is drawn over one with a lower one.
        int compindex = 0;
        // Whether Neovim shows the float: win_float_pos shows it, win_hide
        // hides it without destroying it.
        bool visible = false;
        // The overlay, created once the size of the float's grid is known.
        QPointer<FloatWidget> widget;
    };

    // A highlight definition from Neovim (hl_attr_define), in the form the
    // float overlays are drawn with.
    struct HlAttr {
        // Invalid colors are Neovim's defaults (default_colors_set).
        QColor foreground;
        QColor background;
        int blend = 0;
        bool reverse = false;
        bool bold = false;
        bool italic = false;
        bool underline = false;
    };

    // The state of a grid whose events arrived before the event that says
    // what the grid is: it is remembered until then. Returns null for the
    // screen (grid 1), which is not drawn by the layer.
    FloatState *rememberGrid(int grid);
    // The overlay of a float, created when it is first needed, with the size
    // its grid was resized to before that (see mGridSizes).
    FloatWidget *floatWidget(int grid);
    // The cell a grid_line cell is drawn as: the colors and style come from
    // the cell's highlight and Neovim's defaults.
    FloatCell floatCell(int hlId, const QString &text) const;

    QMap<int, FloatState> mFloats;
    QMap<int, HlAttr> mHlAttrs;
    // The size every grid last reported (grid_resize, by grid id). A float's
    // grid can be resized before the win_float_pos that tells the layer it is
    // a float: the size is remembered until the float's overlay is created.
    QMap<int, QSize> mGridSizes;
    QColor mDefaultForeground = Qt::black;
    QColor mDefaultBackground = Qt::white;
};

} // namespace Internal
} // namespace QNVim
