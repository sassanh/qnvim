#pragma once

#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/editormanager/ieditorfactory.h>
#include <coreplugin/idocument.h>

#include <QPixmap>

#include <memory>
#include <QPointer>
#include <QWidget>

namespace QNVim {
namespace Gui {

class NvimGuiSession;

// A file shown by the embedded Neovim. The text lives in Neovim's buffer; this
// document only mirrors the file name and the 'modified' flag, and forwards
// saving and closing to Neovim, so Qt Creator's document handling (save on
// build, "save all", the modified marker, the close prompt) works.
class NvimDocument final : public Core::IDocument {
    Q_OBJECT

  public:
    explicit NvimDocument(NvimGuiSession *session);
    ~NvimDocument() override;

    Utils::Result<> open(const Utils::FilePath &filePath,
                         const Utils::FilePath &realFilePath) override;

    // An editor without a file ("Show Neovim" with nothing open).
    Utils::Result<> setContents(const QByteArray &contents) override;

    bool isModified() const override { return m_modified; }
    bool isSaveAsAllowed() const override { return false; }
    bool shouldAutoSave() const override { return false; }

    ReloadBehavior reloadBehavior(ChangeTrigger state, ChangeType type) const override;
    Utils::Result<> reload(ReloadFlag flag, ChangeType type) override;

    // Called by the session when Neovim reports a change.
    void setModifiedFromNvim(bool modified);

    int line() const { return m_line; }
    int column() const { return m_column; }
    void setCursorFromNvim(int line, int column);

    // Set while Neovim itself deletes the buffer, so closing the document does
    // not ask Neovim to delete it again.
    bool closingFromNvim = false;

  protected:
    Utils::Result<> saveImpl(const Utils::FilePath &filePath, SaveOption option) override;

  private:
    QPointer<NvimGuiSession> m_session;
    bool m_modified = false;
    int m_line = 1;
    int m_column = 0;
};

// The widget an NvimEditor puts into Qt Creator's editor view. Only one of
// them can hold the Neovim shell at a time (Neovim has a single screen); the
// others show the last picture of it until they become current again.
class NvimEditorHost final : public QWidget {
    Q_OBJECT

  public:
    explicit NvimEditorHost(QWidget *parent = nullptr);
    ~NvimEditorHost() override;

    void adopt(QWidget *shell);
    void release();
    bool holdsShell() const { return m_shell != nullptr; }

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    QPointer<QWidget> m_shell;
    QPixmap m_snapshot;
};

class NvimEditor final : public Core::IEditor {
    Q_OBJECT

  public:
    NvimEditor(NvimGuiSession *session, NvimDocument *document);
    ~NvimEditor() override;

    Core::IDocument *document() const override { return m_document.get(); }
    QWidget *toolBar() override { return nullptr; }

    int currentLine() const override;
    int currentColumn() const override;
    void gotoLine(int line, int column = 0, bool centerLine = true) override;

    NvimEditorHost *host() const;
    NvimDocument *nvimDocument() const { return m_document.get(); }

  private:
    QPointer<NvimGuiSession> m_session;
    std::unique_ptr<NvimDocument> m_document; // one editor per document, like ImageViewer
    NvimEditorHost *m_host = nullptr;          // owned; deleted after releasing the shell
};

class NvimEditorFactory final : public Core::IEditorFactory {
  public:
    explicit NvimEditorFactory(NvimGuiSession *session);

    // Exposed so the session can extend the list after all plugins loaded.
    using Core::IEditorFactory::addMimeType;
    using Core::IEditorFactory::setMimeTypes;
};

} // namespace Gui
} // namespace QNVim
