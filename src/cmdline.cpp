// SPDX-FileCopyrightText: 2023 Mikhail Zolotukhin <mail@gikari.com>
// SPDX-License-Identifier: MIT
#include "cmdline.h"

#include "log.h"
#include "qnvimcore.h"
#include "textediteventfilter.h"

#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QDebug>
#include <QFuture>
#include <QRegularExpression>

#include <texteditor/fontsettings.h>
#include <texteditor/texteditorsettings.h>
#include <coreplugin/editormanager/editormanager.h>

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
        });

    m_core = qobject_cast<QNVimCore*>(parent);
}

void CmdLine::onCmdLineShow(QStringView content,
                            int pos, QChar firstc, QStringView prompt, int indent) {
    // Save params for other requests for cmdline
    m_firstChar = firstc;
    m_prompt = prompt.toString();
    m_indent = indent;

    // Hide all cmds, that could possibly be in other splits
    for (auto &[_, cmdWidget] : m_uniqueWidgets)
        cmdWidget->hide();

    auto currentCmdWidget = currentWidget();

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

void CmdLine::onCmdLineHide()
{
    auto currentCmd = currentWidget();

    currentCmd->clear();
    currentCmd->hide();

    // Focus editor, since we are done
    auto currentEditor = Core::EditorManager::currentEditor();
    if (currentEditor && currentEditor->widget())
        currentEditor->widget()->setFocus();
}

void CmdLine::onCmdLinePos(int pos)
{
    auto currentCmd = currentWidget();

    // Update cursor position
    auto cursor = currentCmd->textCursor();
    auto cursorPositionFromNvim = m_firstChar.isPrint() + m_prompt.length() + m_indent + pos;
    cursor.setPosition(cursorPositionFromNvim);
    currentCmd->setTextCursor(cursor);
}

void CmdLine::showMessage(QStringView message)
{
    auto currentCmd = currentWidget();

    currentCmd->clear();
    currentCmd->setReadOnly(true);
    currentCmd->setText(message.toString());

    currentCmd->show();
}

void CmdLine::clear()
{
    auto currentCmd = currentWidget();

    currentCmd->clear();
}

void CmdLine::editorOpened(Core::IEditor &editor) {
    qDebug(Main) << "CmdLine::editorOpened" << &editor;
    if (!m_widgets.contains(&editor)) {
        auto editorWidget = editor.widget();
        auto stackLayout = editorWidget->parentWidget();
        auto editorView = stackLayout->parentWidget();

        CmdLineWidget* widgetToAdd = nullptr;

        if (auto it = m_uniqueWidgets.find(editorView); it != m_uniqueWidgets.end())
            widgetToAdd = it->second; // We already have a widget for that editor
        else
            widgetToAdd = new CmdLineWidget(m_core, editorView);

        m_widgets.insert({&editor, widgetToAdd});
        m_uniqueWidgets.insert_or_assign(editorView, std::move(widgetToAdd));
    }
}

CmdLineWidget *CmdLine::currentWidget() const {
    auto currentEditor = Core::EditorManager::currentEditor();

    if (!currentEditor)
        return nullptr;

    auto it = m_widgets.find(currentEditor);

    if (it == m_widgets.cend())
        return nullptr;

    return it->second;
}

CmdLineWidget::CmdLineWidget(QNVimCore *core, QWidget *parent) : QWidget(parent) {
    auto parentLayout = parent->layout();
    parentLayout->addWidget(this);

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

void CmdLineWidget::setText(const QString &text)
{
    m_pTextWidget->setPlainText(text);
}

QString CmdLineWidget::text() const
{
    return m_pTextWidget->toPlainText();
}

void CmdLineWidget::clear()
{
    m_pTextWidget->clear();
}

void CmdLineWidget::focus() const
{
    m_pTextWidget->setFocus();
}

void CmdLineWidget::setTextCursor(const QTextCursor &cursor)
{
    m_pTextWidget->setTextCursor(cursor);
}

QTextCursor CmdLineWidget::textCursor() const
{
    return m_pTextWidget->textCursor();
}

void CmdLineWidget::setReadOnly(bool value)
{
    m_pTextWidget->setReadOnly(value);
}

void CmdLineWidget::adjustSize(const QSizeF &newTextDocumentSize)
{
    auto fontHeight = m_pTextWidget->fontMetrics().height();
    auto newHeight = newTextDocumentSize.height() * fontHeight + m_pTextWidget->frameWidth() * 2;

    m_pTextWidget->setMaximumHeight(newHeight);
    m_pTextWidget->parentWidget()->setMaximumHeight(newHeight);
}

bool ParentChangedFilter::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::ParentChange)
        emit parentChanged(watched->parent());

    return QObject::eventFilter(watched, event);
}

} // namespace QNVim::Internal
