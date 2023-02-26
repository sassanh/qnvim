// SPDX-FileCopyrightText: 2023 Mikhail Zolotukhin <mail@gikari.com>
// SPDX-License-Identifier: MIT
#include "textediteventfilter.h"

#include <QKeyEvent>
#include <QPlainTextEdit>

#include <input.h>

#include <texteditor/texteditor.h>

namespace QNVim::Internal {

TextEditEventFilter::TextEditEventFilter(NeovimQt::NeovimConnector *nvim, QObject *parent)
    : QObject(parent), m_nvim(nvim) {
}

bool TextEditEventFilter::eventFilter(QObject *watched, QEvent *event)
{
    if (qobject_cast<TextEditor::TextEditorWidget *>(watched) ||
        qobject_cast<QPlainTextEdit *>(watched)) {
        if (event->type() == QEvent::Resize) {
            emit resizeNeeded();
            return false;
        }
    }

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        QString key = NeovimQt::Input::convertKey(*keyEvent);
        m_nvim->api6()->nvim_input(key.toUtf8());
        return true;
    } else if (event->type() == QEvent::ShortcutOverride) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        QString key = NeovimQt::Input::convertKey(*keyEvent);
        if (keyEvent->key() == Qt::Key_Escape) {
            m_nvim->api6()->nvim_input(key.toUtf8());
        } else {
            keyEvent->accept();
        }
        return true;
    }
    return false;
}

} // namespace QNVim::Internal
