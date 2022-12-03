// SPDX-FileCopyrightText: 2018-2019 Sassan Haradji <sassanh@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <QWidget>

namespace TextEditor {
class TextEditorWidget;
}

namespace QNVim {
namespace Internal {

class NumbersColumn : public QWidget {
    Q_OBJECT
    bool mNumber = false;
    TextEditor::TextEditorWidget *mEditor = nullptr;

  public:
    NumbersColumn();

    void setEditor(TextEditor::TextEditorWidget *);
    void setNumber(bool);
    void updateGeometry();

  protected:
    void paintEvent(QPaintEvent *event);
    bool eventFilter(QObject *, QEvent *);
};

} // namespace Internal
} // namespace QNVim
