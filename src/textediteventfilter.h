#pragma once

#include <NeovimConnector.h>
#include <QObject>

namespace QNVim::Internal {

class TextEditEventFilter : public QObject {
    Q_OBJECT
  public:
    explicit TextEditEventFilter(NeovimQt::NeovimConnector *nvim, QObject *parent = nullptr);

    bool eventFilter(QObject *watched, QEvent *event) override;
  signals:
    void resizeNeeded();

  private:
    NeovimQt::NeovimConnector *m_nvim;
};

} // namespace QNVim::Internal
