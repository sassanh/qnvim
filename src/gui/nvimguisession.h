#pragma once

#include <utils/filepath.h>

#include <QElapsedTimer>
#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QVariantList>

#include <functional>
#include <memory>

namespace Core {
class IEditor;
}

namespace NeovimQt {
class NeovimConnector;
class Shell;
} // namespace NeovimQt

namespace QNVim {
namespace Gui {

class NvimDocument;
class NvimEditor;
class NvimEditorFactory;
class NvimEditorHost;

// GUI mode: one embedded Neovim, drawn by neovim-qt's Shell widget inside Qt
// Creator's editor area.
//
// Neovim renders everything itself (windows, statusline, command line,
// messages, completion menus, floating windows of plugins), exactly as in a
// terminal or nvim-qt, so every plugin works unchanged. Qt Creator gets one
// lightweight editor per file buffer, which keeps its document-centric
// features working: the open documents list, "save all"/save before build,
// the locator, project tree, compile output and debugger opening files at a
// line, and the current file for VCS and build actions.
class NvimGuiSession final : public QObject {
    Q_OBJECT

  public:
    NvimGuiSession();
    ~NvimGuiSession() override;

    static NvimGuiSession *instance();

    // Makes the factory the preferred editor for text files. Must run before
    // Core's extensionsInitialized() (which reads the setting), i.e. in this
    // plugin's extensionsInitialized().
    void takeOverTextEditors();
    // Removes everything takeOverTextEditors() wrote (classic mode).
    static void releaseTextEditors();

    void shutdown();
    void restart();

    // Neovim side, used by documents and editors.
    void documentOpened(NvimDocument *document);
    void documentClosing(NvimDocument *document);
    bool writeBuffer(const Utils::FilePath &path, QString *error);
    void checkTime();
    void gotoPosition(const Utils::FilePath &path, int line, int column, bool center);

    QWidget *shellWidget() const;

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void start();
    void onNeovimReady();
    void onNeovimExited();
    void onNotification(const QByteArray &name, const QVariantList &args);
    void onCurrentEditorChanged(Core::IEditor *editor);
    void onBufEnter(const Utils::FilePath &path);
    void onBufDelete(const Utils::FilePath &path);
    void attachShellTo(NvimEditorHost *host);
    void sendActions();
    void syncWorkingDirectory();
    void applyFont();
    void triggerAction(const QByteArray &id);
    void luaCall(const QByteArray &code, const QVariantList &args = {});
    bool isPassthrough(const QKeySequence &seq) const;

    NvimDocument *documentFor(const Utils::FilePath &path) const;
    static QString key(const Utils::FilePath &path);

    std::unique_ptr<NvimEditorFactory> m_factory;
    QPointer<NeovimQt::NeovimConnector> m_nvim;
    QPointer<NeovimQt::Shell> m_shell;
    QPointer<QWidget> m_parking; // hidden parent of the shell while no editor holds it
    QPointer<NvimEditorHost> m_currentHost;
    QHash<QString, NvimDocument *> m_documents;
    Utils::FilePath m_nvimCurrentFile;
    QList<QKeySequence> m_passthrough;
    QString m_runtimeFile;
    bool m_ready = false;
    bool m_syncingFromNvim = false;
    bool m_shuttingDown = false;
    bool m_restartRequested = false;
    QElapsedTimer m_startTimer;
    QList<std::function<void()>> m_pending; // calls queued until Neovim is ready
};

} // namespace Gui
} // namespace QNVim
