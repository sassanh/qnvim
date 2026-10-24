#pragma once

#include <QStringView>

namespace QNVim {
namespace Internal {

// Conversions between Qt's 0-based UTF-16 offsets and Neovim's 1-based lines
// with 1-based UTF-8 byte columns.
struct LineColumn {
    int line;
    int column;
};

LineColumn lineColumn(QStringView text, int utf16Position);
int lineStart(QStringView text, int line);
int lineEnd(QStringView text, int line);
int positionFor(QStringView text, int line, int byteColumn);

} // namespace Internal
} // namespace QNVim
