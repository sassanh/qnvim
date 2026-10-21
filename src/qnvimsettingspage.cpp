#include "qnvimsettingspage.h"

#include "qnvimconstants.h"
#include "qnvimsettings.h"

#include <QCoreApplication>

namespace QNVim {
namespace Internal {

QNVimSettingsPage::QNVimSettingsPage() {
    setId(Constants::SETTINGS_PAGE_ID);
    setDisplayName(QCoreApplication::translate("QNVim", "General"));
    setCategory(Constants::SETTINGS_CATEGORY_ID);
    setSettingsProvider([] { return &settings(); });
}

} // namespace Internal
} // namespace QNVim
