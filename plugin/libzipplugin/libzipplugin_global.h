#ifndef LIBZIPPLUGIN_GLOBAL_H
#define LIBZIPPLUGIN_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(LIBZIPPLUGIN_LIBRARY)
#  define LIBZIPPLUGINSHARED_EXPORT Q_DECL_EXPORT
#else
#  define LIBZIPPLUGINSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // LIBZIPPLUGIN_GLOBAL_H
