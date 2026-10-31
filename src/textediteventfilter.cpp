#include "textediteventfilter.h"

#include "qnvimcore.h"

#include <QKeyEvent>
#include <QPlainTextEdit>

#include <input.h>
#include <neovimconnector.h>

#include <texteditor/texteditor.h>

namespace QNVim::Internal {

TextEditEventFilter::TextEditEventFilter(QNVimCore *core, QObject *parent)
    : QObject(parent), m_core(core) {
}

bool TextEditEventFilter::eventFilter(QObject *watched, QEvent *event)
{
    // Ask for the current connector: a restart replaces it, and the widget
    // outlives the old connection.
    NeovimQt::NeovimConnector *nvim = m_core ? m_core->nvimConnector() : nullptr;
    if (!nvim or !nvim->isReady())
        return false;

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
        nvim->api6()->nvim_input(key.toUtf8());
        return true;
    } else if (event->type() == QEvent::ShortcutOverride) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        QString key = NeovimQt::Input::convertKey(*keyEvent);
        if (keyEvent->key() == Qt::Key_Escape) {
            nvim->api6()->nvim_input(key.toUtf8());
        } else {
            keyEvent->accept();
        }
        return true;
    }
    return false;
}

} // namespace QNVim::Internal
