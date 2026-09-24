#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#else
#include "../ctp2_code/mapgen/mappluginloader.h"
#endif

int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    // World generation unloads each pass and can reopen the same plugin later.
    for (int pass = 0; pass < 8; ++pass) {
        for (int i = 1; i < argc; ++i) {
#ifdef _WIN32
            HMODULE plugin = LoadLibraryA(argv[i]);
            if (!plugin || !GetProcAddress(plugin, "CoCreateMapGenerator")) return 2;
            if (!FreeLibrary(plugin)) return 3;
#else
            void *plugin = ctp2_OpenMapPlugin(argv[i]);
            if (!plugin) { std::fprintf(stderr, "%s\n", dlerror()); return 2; }
            if (!dlsym(plugin, "CoCreateMapGenerator")) return 3;
            if (dlclose(plugin)) return 4;
#endif
        }
    }
    return 0;
}
