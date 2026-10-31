#pragma once

#include <QObject>
#include <QPointer>

namespace NeovimQt {
class NeovimConnector;
}

namespace QNVim::Internal {

class QNVimCore;

class TextEditEventFilter : public QObject {
    Q_OBJECT
  public:
    explicit TextEditEventFilter(QNVimCore *core, QObject *parent = nullptr);

    bool eventFilter(QObject *watched, QEvent *event) override;

  signals:
    void resizeNeeded();

  private:
    // The command line widget outlives the core during shutdown; a QPointer
    // becomes null when the core is destroyed.
    QPointer<QNVimCore> m_core;
};

} // namespace QNVim::Internal
