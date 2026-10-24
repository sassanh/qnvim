#include "nvimguieditor.h"

#include "nvimguiconstants.h"
#include "nvimguisession.h"

#include <utils/mimeutils.h>

#include <QCoreApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QVBoxLayout>

namespace QNVim {
namespace Gui {

namespace {
QString tr(const char *text) {
    return QCoreApplication::translate("QNVim", text);
}
} // namespace

// NvimDocument ---------------------------------------------------------------

NvimDocument::NvimDocument(NvimGuiSession *session) : m_session(session) {
    setId(Constants::DOCUMENT_ID);
    setMimeType(QStringLiteral("text/plain"));
}

NvimDocument::~NvimDocument() {
    if (m_session)
        m_session->documentClosing(this);
}

Utils::Result<> NvimDocument::open(const Utils::FilePath &filePath,
                                   const Utils::FilePath &realFilePath) {
    Q_UNUSED(realFilePath)
    if (!m_session)
        return Utils::ResultError(tr("Neovim is not running."));
    setFilePath(filePath.absoluteFilePath());
    const Utils::MimeType mime = Utils::mimeTypeForFile(filePath);
    if (mime.isValid())
        setMimeType(mime.name());
    m_session->documentOpened(this);
    return Utils::ResultOk;
}

Utils::Result<> NvimDocument::setContents(const QByteArray &contents) {
    Q_UNUSED(contents)
    setTemporary(true);
    return Utils::ResultOk;
}

Core::IDocument::ReloadBehavior NvimDocument::reloadBehavior(ChangeTrigger state,
                                                             ChangeType type) const {
    Q_UNUSED(state)
    Q_UNUSED(type)
    // Neovim notices external changes itself ('autoread', :checktime, W11/W12
    // prompts); Qt Creator must not ask a second time.
    return BehaviorSilent;
}

Utils::Result<> NvimDocument::reload(ReloadFlag flag, ChangeType type) {
    Q_UNUSED(type)
    if (flag == FlagReload && m_session)
        m_session->checkTime();
    return Utils::ResultOk;
}

void NvimDocument::setModifiedFromNvim(bool modified) {
    if (m_modified == modified)
        return;
    m_modified = modified;
    emit changed();
}

void NvimDocument::setCursorFromNvim(int line, int column) {
    m_line = line;
    m_column = column;
}

Utils::Result<> NvimDocument::saveImpl(const Utils::FilePath &filePath, SaveOption option) {
    Q_UNUSED(option)
    if (!m_session)
        return Utils::ResultError(tr("Neovim is not running."));
    if (!filePath.isEmpty() && filePath != this->filePath())
        return Utils::ResultError(tr("Use :saveas in Neovim to save under another name."));
    QString error;
    if (!m_session->writeBuffer(this->filePath(), &error))
        return Utils::ResultError(error);
    setModifiedFromNvim(false);
    return Utils::ResultOk;
}

// NvimEditorHost -------------------------------------------------------------

NvimEditorHost::NvimEditorHost(QWidget *parent) : QWidget(parent) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

NvimEditorHost::~NvimEditorHost() {
    // The shell belongs to the session, never to an editor.
    release();
}

void NvimEditorHost::adopt(QWidget *shell) {
    if (!shell || m_shell == shell)
        return;
    m_shell = shell;
    m_snapshot = {};
    shell->setParent(this);
    layout()->addWidget(shell);
    setFocusProxy(shell);
    shell->show();
}

void NvimEditorHost::release() {
    if (!m_shell)
        return;
    // Keep a picture for when this editor is visible in another split.
    if (m_shell->isVisible() && m_shell->width() > 0 && m_shell->height() > 0)
        m_snapshot = m_shell->grab();
    layout()->removeWidget(m_shell);
    setFocusProxy(nullptr);
    m_shell->hide();
    m_shell->setParent(nullptr);
    m_shell = nullptr;
    update();
}

void NvimEditorHost::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.fillRect(event->rect(), palette().base());
    if (m_shell)
        return;
    if (!m_snapshot.isNull()) {
        painter.drawPixmap(0, 0, m_snapshot);
        painter.fillRect(rect(), QColor(0, 0, 0, 90));
    }
    painter.setPen(palette().text().color());
    painter.drawText(rect(), Qt::AlignCenter,
                     tr("Neovim is shown in the active editor.\nClick here to show it here."));
}

// NvimEditor -----------------------------------------------------------------

NvimEditor::NvimEditor(NvimGuiSession *session, NvimDocument *document)
    : m_session(session), m_document(document), m_host(new NvimEditorHost) {
    setWidget(m_host);
    setContext(Core::Context(Constants::EDITOR_ID));
    setDuplicateSupported(false);
}

NvimEditor::~NvimEditor() {
    m_host->release();
    delete m_host;
}

NvimEditorHost *NvimEditor::host() const {
    return m_host;
}

int NvimEditor::currentLine() const {
    return m_document->line();
}

int NvimEditor::currentColumn() const {
    return m_document->column();
}

void NvimEditor::gotoLine(int line, int column, bool centerLine) {
    m_document->setCursorFromNvim(line, column);
    if (m_session)
        m_session->gotoPosition(m_document->filePath(), line, column, centerLine);
}

// NvimEditorFactory ----------------------------------------------------------

NvimEditorFactory::NvimEditorFactory(NvimGuiSession *session) {
    setId(Constants::EDITOR_ID);
    setDisplayName(tr(Constants::EDITOR_DISPLAY_NAME));
    // text/plain makes the editor available ("Open With") for every text file;
    // takeOverTextEditors() makes it the default for the specific types.
    addMimeType(QStringLiteral("text/plain"));
    setEditorCreator([session] {
        return new NvimEditor(session, new NvimDocument(session));
    });
}

} // namespace Gui
} // namespace QNVim
