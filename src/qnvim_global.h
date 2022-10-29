#pragma once

#include <QtGlobal>

#if defined(QNVIM_LIBRARY)
#  define QNVIMSHARED_EXPORT Q_DECL_EXPORT
#else
#  define QNVIMSHARED_EXPORT Q_DECL_IMPORT
#endif
