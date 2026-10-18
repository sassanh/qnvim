#pragma once

#include <QPointer>

#include <map>

#include <coreplugin/editormanager/ieditor.h>

QT_BEGIN_NAMESPACE
class QPlainTextEdit;
class QTextCursor;
QT_END_NAMESPACE

namespace NeovimQt {
class NeovimConnector;
}

namespace QNVim::Internal {

class QNVimCore;

class CmdLineWidget : public QWidget {
    Q_OBJECT
  public:
    explicit CmdLineWidget(QNVimCore *core, QWidget *parent = nullptr);

    // False if the widget failed to embed itself into the editor view
    bool isValid() const { return m_pTextWidget != nullptr; }

    void setText(const QString &text);
    QString text() const;
    void clear();

    void focus() const;

    void setTextCursor(const QTextCursor &cursor);
    QTextCursor textCursor() const;

    void setReadOnly(bool value);

  private:
    void adjustSize(const QSizeF &newSize);

    QPlainTextEdit *m_pTextWidget = nullptr;
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
    CmdLineWidget *currentWidget() const;
    CmdLineWidget *ensureCurrentWidget();

    QNVimCore *m_core{};

    // ':', '/' or '?' char in the beginning
    QChar m_firstChar{};

    // vim.input() prompt, usually used in Neovim plugins
    QString m_prompt{};

    // How many spaces the content of the cmdline should be indented
    int m_indent{};

    // Maps every editor to its CmdLine widget. The widgets are QPointers,
    // because a widget is destroyed together with the editor view it is
    // embedded into (Qt Creator recreates editor views, e.g. while restoring a
    // session), and the map must not keep dangling pointers.
    std::map<Core::IEditor *, QPointer<CmdLineWidget>> m_widgets;

    // One CmdLine widget per editor view (split), shared across editors in that view
    std::map<QWidget *, QPointer<CmdLineWidget>> m_uniqueWidgets;
};

class ParentChangedFilter : public QObject {
    Q_OBJECT
  public:
    explicit ParentChangedFilter(QObject *parent = nullptr) : QObject(parent) {}

  signals:
    void parentChanged(QObject *parent);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};
} // namespace QNVim::Internal
