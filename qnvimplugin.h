#pragma once

#include "qnvim_global.h"
#include <QPoint>
#include <QRect>

#include <extensionsystem/iplugin.h>

namespace Core {
class IEditor;
}

namespace NeovimQt {
class NeovimConnector;
}

namespace QNVim {
namespace Internal {

class QNVimPlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "QNVim.json")

public:
    QNVimPlugin();
    ~QNVimPlugin();

    bool initialize(const QStringList &, QString *);
    bool initialize();
    void extensionsInitialized();
    ShutdownFlag aboutToShutdown();

    bool eventFilter(QObject *, QEvent *);

private:
    void toggleQNVim();

    void editorOpened(Core::IEditor *);
    void editorAboutToClose(Core::IEditor *);

    void handleNotification(const QByteArray &, const QVariantList &);
    void redraw(const QVariantList &);

    bool mEnabled;

    NeovimQt::NeovimConnector *mNVim;
    QMap<QString, Core::IEditor *> mEditors;
    QStringList mContent;

    unsigned mWidth, mHeight;
    bool mBusy;
    bool mMouse;
    QByteArray mMode;
    QPoint mCursor;

    QRect mScrollRegion;
};

} // namespace Internal
} // namespace QNVim
