#include "cmdline.h"

#include "log.h"
#include "qnvimcore.h"
#include "textediteventfilter.h"

#include <QDebug>
#include <QEvent>
#include <QFuture>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QRegularExpression>

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icore.h>
#include <texteditor/fontsettings.h>
#include <texteditor/texteditorsettings.h>

#include <neovimconnector.h>

namespace QNVim::Internal {

CmdLine::CmdLine(QObject *parent) : QObject(parent) {
    // HACK:
    // We need the parent of an editor, so that we can inject CmdLine widget below it.
    // Parent widget might be absent when EditorManager::editorOpened is emitted.
    // Because of that we detect when the parent changes (from nothing to something)
    // and only then call our editorOpened callback
    connect(
        Core::EditorManager::instance(), &Core::EditorManager::editorOpened, this,
        [this](Core::IEditor *editor) {
            if (!editor)
                return;

            auto editorWidget = editor->widget();
            auto eventFilter = new ParentChangedFilter(editorWidget);
            editorWidget->installEventFilter(eventFilter);

            connect(
                eventFilter, &ParentChangedFilter::parentChanged, this,
                [this, editor](QObject *parent) {
                    this->editorOpened(*editor);
                },
                Qt::SingleShotConnection);

            // The widget hierarchy can already be complete when the editor is
            // opened, in which case no ParentChange will follow. Try right
            // away; editorOpened() is idempotent and retries internally when
            // the editor view is still missing.
            this->editorOpened(*editor);
        });

    m_core = qobject_cast<QNVimCore *>(parent);
}

void CmdLine::onCmdLineShow(QStringView content,
                            int pos, QChar firstc, QStringView prompt, int indent) {
    // Save params for other requests for cmdline
    m_firstChar = firstc;
    m_prompt = prompt.toString();
    m_indent = indent;

    // Hide all cmds, that could possibly be in other splits
    for (auto &[_, cmdWidget] : m_uniqueWidgets)
        if (cmdWidget)
            cmdWidget->hide();

    auto currentCmdWidget = ensureCurrentWidget();

    if (!currentCmdWidget)
        return;

    QString text = firstc + prompt + QString(indent, ' ') + content;

    currentCmdWidget->setText(text);
    currentCmdWidget->setReadOnly(false);
    currentCmdWidget->show();

    currentCmdWidget->focus();

    // Update cursor position
    auto cursor = currentCmdWidget->textCursor();
    auto cursorPositionFromNvim = firstc.isPrint() + prompt.length() + indent + pos;
    cursor.setPosition(cursorPositionFromNvim);
    currentCmdWidget->setTextCursor(cursor);
}

void CmdLine::onCmdLineHide() {
    // Hide all cmds, that could possibly be in other splits. No widget is
    // created here: if there is none, there is nothing to hide.
    for (auto &[_, cmdWidget] : m_uniqueWidgets) {
        if (!cmdWidget)
            continue;

        cmdWidget->clear();
        cmdWidget->hide();
    }

    // Focus editor, since we are done
    auto currentEditor = Core::EditorManager::currentEditor();
    if (currentEditor && currentEditor->widget())
        currentEditor->widget()->setFocus();
}

void CmdLine::onCmdLinePos(int pos) {
    auto currentCmd = ensureCurrentWidget();

    if (!currentCmd)
        return;

    // Update cursor position
    auto cursor = currentCmd->textCursor();
    auto cursorPositionFromNvim = m_firstChar.isPrint() + m_prompt.length() + m_indent + pos;
    cursor.setPosition(cursorPositionFromNvim);
    currentCmd->setTextCursor(cursor);
}

void CmdLine::showMessage(QStringView message) {
    auto currentCmd = ensureCurrentWidget();

    if (!currentCmd)
        return;

    currentCmd->clear();
    currentCmd->setReadOnly(true);
    currentCmd->setText(message.toString());

    currentCmd->show();
}

void CmdLine::clear() {
    // Messages can be shown on any of the widgets (e.g. on the widget of a
    // buffer whose editor was just closed), so clear all of them. Message
    // widgets are read-only, hide them; a command line being edited must stay
    // visible.
    for (auto &[_, cmdWidget] : m_uniqueWidgets) {
        if (!cmdWidget)
            continue;

        cmdWidget->clear();
        if (cmdWidget->isReadOnly())
            cmdWidget->hide();
    }
}

void CmdLine::editorOpened(Core::IEditor &editor) {
    qDebug(Main) << "CmdLine::editorOpened" << &editor;
    if (auto it = m_widgets.find(&editor);
        it != m_widgets.end() && it->second && it->second->isValid())
        return; // The editor already has a working CmdLine widget

    auto editorWidget = editor.widget();
    if (!editorWidget)
        return;

    auto stackLayout = editorWidget->parentWidget();
    if (!stackLayout)
        return;

    auto editorView = stackLayout->parentWidget();
    if (!editorView) {
        // The stack widget hasn't been parented into the editor view yet.
        // Install a filter on it and retry when its parent is set.
        auto editorPtr = &editor;
        auto filter = new ParentChangedFilter(stackLayout);
        stackLayout->installEventFilter(filter);
        connect(
            filter, &ParentChangedFilter::parentChanged, this,
            [this, editorPtr](QObject *) { this->editorOpened(*editorPtr); },
            Qt::SingleShotConnection);
        return;
    }

    CmdLineWidget *widgetToAdd = nullptr;

    if (auto it = m_uniqueWidgets.find(editorView);
        it != m_uniqueWidgets.end() && it->second && it->second->isValid())
        widgetToAdd = it->second; // We already have a widget for that editor view
    else {
        // The command line is an overlay in the main window, so that it is
        // drawn above Qt Creator's status bar. A placeholder in the editor
        // view keeps the space it takes from the text area.
        QWidget *overlayParent = Core::ICore::mainWindow();
        if (!overlayParent)
            overlayParent = editorView->window();
        widgetToAdd = new CmdLineWidget(m_core, editorView, overlayParent);
        if (!widgetToAdd->isValid()) {
            // The editor view is not ready to embed the widget yet (its layout
            // is missing). Try again the next time a CmdLine widget is needed.
            widgetToAdd->deleteLater();
            return;
        }
    }

    // insert_or_assign: an entry may exist from an earlier editor whose widget
    // was destroyed (e.g. when its editor view was recreated).
    m_widgets.insert_or_assign(&editor, widgetToAdd);
    m_uniqueWidgets.insert_or_assign(editorView, widgetToAdd);
}

CmdLineWidget *CmdLine::currentWidget() const {
    auto currentEditor = Core::EditorManager::currentEditor();

    if (!currentEditor)
        return nullptr;

    auto it = m_widgets.find(currentEditor);

    if (it == m_widgets.cend() || !it->second || !it->second->isValid())
        return nullptr;

    return it->second;
}

CmdLineWidget *CmdLine::ensureCurrentWidget() {
    if (auto widget = currentWidget())
        return widget;

    auto currentEditor = Core::EditorManager::currentEditor();

    if (!currentEditor)
        return nullptr;

    // No widget for the current editor: either none was created (e.g. its
    // editor view has just been created) or the widget was destroyed together
    // with its editor view. Create it now that the hierarchy is in place.
    editorOpened(*currentEditor);
    return currentWidget();
}

CmdLineWidget::CmdLineWidget(QNVimCore *core, QWidget *editorView, QWidget *overlayParent)
    : QWidget(overlayParent), m_editorView(editorView) {
    auto parentLayout = editorView ? editorView->layout() : nullptr;
    if (!parentLayout || !overlayParent)
        return;

    // The visible widget is an overlay in the main window: a child of the
    // editor view would be drawn below the status bar when Qt Creator draws
    // the status bar over the bottom of the editor area. The placeholder is
    // invisible but keeps the command line's space in the view's layout.
    m_placeholder = new QWidget(editorView);
    m_placeholder->setObjectName(QStringLiteral("cmdline-placeholder"));
    m_placeholder->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_placeholder->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_placeholder->installEventFilter(this);
    parentLayout->addWidget(m_placeholder);
    m_placeholder->hide();

    editorView->installEventFilter(this);
    connect(editorView, &QObject::destroyed, this, &QObject::deleteLater);

    auto pLayout = new QHBoxLayout(this);
    pLayout->setContentsMargins(0, 0, 0, 0);

    m_pTextWidget = new QPlainTextEdit(this);
    m_pTextWidget->document()->setDocumentMargin(0);
    m_pTextWidget->setFrameStyle(QFrame::Shape::NoFrame);
    m_pTextWidget->setObjectName(QStringLiteral("cmdline"));
    m_pTextWidget->setStyleSheet(QStringLiteral("#cmdline { border-top: 1px solid palette(dark) }"));

    auto editorFont = TextEditor::TextEditorSettings::instance()->fontSettings().font();
    m_pTextWidget->setFont(editorFont);

    auto textEditEventFilter = new TextEditEventFilter(core->nvimConnector(), this);
    m_pTextWidget->installEventFilter(textEditEventFilter);

    connect(m_pTextWidget->document()->documentLayout(),
            &QAbstractTextDocumentLayout::documentSizeChanged,
            this, &CmdLineWidget::adjustSize);

    connect(TextEditor::TextEditorSettings::instance(),
            &TextEditor::TextEditorSettings::fontSettingsChanged,
            this, [this](const TextEditor::FontSettings &settings) {
                m_pTextWidget->setFont(settings.font());
            });

    adjustSize(m_pTextWidget->document()->size());
    // Do not show by default
    hide();

    pLayout->addWidget(m_pTextWidget);
}

CmdLineWidget::~CmdLineWidget() {
    // The placeholder is a child of the editor view, not of this overlay.
    if (m_placeholder)
        delete m_placeholder;
}

void CmdLineWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);

    if (m_placeholder) {
        m_placeholder->show();
        updateOverlayGeometry();
    }

    // The overlay is a child of the main window, but the status bar was added
    // after it: raise it so that it is drawn above everything.
    raise();
}

void CmdLineWidget::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);

    if (m_placeholder)
        m_placeholder->hide();
}

bool CmdLineWidget::eventFilter(QObject *watched, QEvent *event) {
    if ((watched == m_placeholder || watched == m_editorView)
        && (event->type() == QEvent::Move || event->type() == QEvent::Resize
            || event->type() == QEvent::Show || event->type() == QEvent::Hide
            || event->type() == QEvent::ParentChange)) {
        updateOverlayGeometry();
    }

    return QWidget::eventFilter(watched, event);
}

void CmdLineWidget::updateOverlayGeometry() {
    if (!m_placeholder || !m_placeholder->isVisible() || !isVisible() || !parentWidget())
        return;

    const QPoint topLeft = m_placeholder->mapTo(parentWidget(), QPoint(0, 0));
    setGeometry(QRect(topLeft, m_placeholder->size()));
}

void CmdLineWidget::setText(const QString &text) {
    m_pTextWidget->setPlainText(text);
}

QString CmdLineWidget::text() const {
    return m_pTextWidget->toPlainText();
}

void CmdLineWidget::clear() {
    m_pTextWidget->clear();
}

void CmdLineWidget::focus() const {
    m_pTextWidget->setFocus();
}

void CmdLineWidget::setTextCursor(const QTextCursor &cursor) {
    m_pTextWidget->setTextCursor(cursor);
}

QTextCursor CmdLineWidget::textCursor() const {
    return m_pTextWidget->textCursor();
}

void CmdLineWidget::setReadOnly(bool value) {
    m_pTextWidget->setReadOnly(value);
}

bool CmdLineWidget::isReadOnly() const {
    return m_pTextWidget->isReadOnly();
}

void CmdLineWidget::adjustSize(const QSizeF &newTextDocumentSize) {
    auto fontHeight = m_pTextWidget->fontMetrics().height();
    auto newHeight = qCeil(newTextDocumentSize.height() * fontHeight)
                     + m_pTextWidget->frameWidth() * 2;

    m_pTextWidget->setMaximumHeight(newHeight);
    setFixedHeight(newHeight);
    if (m_placeholder)
        m_placeholder->setFixedHeight(newHeight);

    updateOverlayGeometry();
}

bool ParentChangedFilter::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::ParentChange)
        emit parentChanged(watched->parent());

    return QObject::eventFilter(watched, event);
}

} // namespace QNVim::Internal
