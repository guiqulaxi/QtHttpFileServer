#ifndef JQLIBRARY_GLOBAL_H
#define JQLIBRARY_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(JQLIBRARY_LIBRARY)
#  define JQLIBRARY_EXPORT Q_DECL_EXPORT
#else
#  define JQLIBRARY_EXPORT Q_DECL_IMPORT
#endif

#endif // JQLIBRARY_GLOBAL_H
