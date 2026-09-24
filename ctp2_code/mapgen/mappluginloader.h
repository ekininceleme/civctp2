#ifndef CTP2_MAPPLUGINLOADER_H
#define CTP2_MAPPLUGINLOADER_H
#ifndef _WIN32
#include <dlfcn.h>

inline void *ctp2_OpenMapPlugin(const char *path)
{
    int flags = RTLD_LAZY | RTLD_LOCAL;
#ifdef __APPLE__
    // World generation reopens the same plugin for multiple passes. On macOS,
    // unloading instrumented images leaves stale ASan global registrations.
    // Keep code/metadata resident; dlclose still balances each acquired handle,
    // and each generator object is released normally by the caller.
    flags |= RTLD_NODELETE;
#endif
    return dlopen(path, flags);
}
#endif
#endif
