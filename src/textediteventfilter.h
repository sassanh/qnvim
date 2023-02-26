// SPDX-FileCopyrightText: 2023 Mikhail Zolotukhin <mail@gikari.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>

namespace NeovimQt {
class NeovimConnector;
}

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
