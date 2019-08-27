#ifndef LIBSINGLEFILEPLUGIN_GLOBAL_H
#define LIBSINGLEFILEPLUGIN_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(LIBSINGLEFILEPLUGIN_LIBRARY)
#  define LIBSINGLEFILEPLUGINSHARED_EXPORT Q_DECL_EXPORT
#else
#  define LIBSINGLEFILEPLUGINSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // LIBSINGLEFILEPLUGIN_GLOBAL_H
