#include "textpos.h"

#include <QByteArray>
#include <QString>
#include <QtGlobal>

namespace QNVim {
namespace Internal {

LineColumn lineColumn(QStringView text, int utf16Position) {
    const int position = qBound(0, utf16Position, int(text.size()));
    const QStringView prefix = text.left(position);
    const int start = int(prefix.lastIndexOf(u'\n')) + 1;
    return {int(prefix.count(u'\n')) + 1,
            int(prefix.mid(start).toUtf8().size()) + 1};
}

int lineStart(QStringView text, int line) {
    int offset = 0;
    for (int current = 1; current < line; ++current) {
        const qsizetype newline = text.indexOf(u'\n', offset);
        if (newline < 0)
            return int(text.size());
        offset = int(newline) + 1;
    }
    return offset;
}

int lineEnd(QStringView text, int line) {
    const qsizetype newline = text.indexOf(u'\n', lineStart(text, line));
    return newline < 0 ? int(text.size()) : int(newline);
}

int positionFor(QStringView text, int line, int byteColumn) {
    const int start = lineStart(text, line);
    const qsizetype newline = text.indexOf(u'\n', start);
    const QStringView lineChars = text.mid(start, (newline < 0 ? text.size() : newline) - start);
    const QByteArray bytes = lineChars.toUtf8().left(qMax(0, byteColumn - 1));
    return start + int(QString::fromUtf8(bytes).size());
}

} // namespace Internal
} // namespace QNVim
