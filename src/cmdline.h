#pragma once

#include <coreplugin/editormanager/ieditor.h>

#include <NeovimConnector.h>

#include "automap.h"

QT_BEGIN_NAMESPACE
class QPlainTextEdit;
class QTextCursor;
QT_END_NAMESPACE

namespace QNVim::Internal {

class QNVimCore;

class CmdLineWidget : public QWidget {
    Q_OBJECT
  public:
    explicit CmdLineWidget(QNVimCore* core, QWidget *parent = nullptr);

    void setText(const QString& text);
    QString text() const;
    void clear();

    void focus() const;

    void setTextCursor(const QTextCursor& cursor);
    QTextCursor textCursor() const;

    void setReadOnly(bool value);

  private:
    void adjustSize(const QSizeF &newSize);

    QPlainTextEdit *m_pTextWidget;
};

class CmdLine : public QObject {
    Q_OBJECT

  public:
    explicit CmdLine(QObject *parent = nullptr);

    /**
     * @sa https://neovim.io/doc/user/ui.html#ui-cmdline
     */
    void onCmdLineShow(QStringView content, int pos, QChar firstc, QStringView prompt, int indent);
    void onCmdLineHide();
    void onCmdLinePos(int pos);

    void showMessage(QStringView message);
    void clear();

  private:
    void editorOpened(Core::IEditor &editor);
    CmdLineWidget* currentWidget() const;

    QNVimCore* m_core {};

    // ':', '/' or '?' char in the beginning
    QChar m_firstChar {};

    // vim.input() prompt, usually used in Neovim plugins
    QString m_prompt {};

    // How many spaces the content of the cmdline should be indented
    int m_indent {};

    // Stores bounds between editors and CmdLineWidgets.
    // The CmdLineWidget we receive depends on
    AutoMap<Core::IEditor*, CmdLineWidget*> m_widgets {};

    // Stores unique bounds between EditorView (which is like a split in Neovim)
    // and CmdLineWidget. EditorView is a private class of QtCreator, therefore
    // we use a QWidget pointer here.
    AutoMap<QWidget*, CmdLineWidget*> m_uniqueWidgets {};
};

class ParentChangedFilter : public QObject {
    Q_OBJECT
  public:
    explicit ParentChangedFilter(QObject *parent = nullptr) : QObject(parent) {}

  signals:
    void parentChanged(QObject* parent);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};
} // namespace QNVim::Internal
