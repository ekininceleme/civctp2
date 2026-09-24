#ifndef CTP2_FREETYPE_LIBRARY_H
#define CTP2_FREETYPE_LIBRARY_H
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H

// The original renderer used full-pixel TrueType hinting. Modern FreeType's
// default interpreter changes advances in the bundled fonts, affecting layout.
inline FT_Error ctp2_InitFontLibrary(FT_Library *library)
{
    FT_Error error = FT_Init_FreeType(library);
    if (error) return error;
    FT_UInt version = 35;
    error = FT_Property_Set(*library, "truetype", "interpreter-version", &version);
    if (error) {
        FT_Done_FreeType(*library);
        *library = nullptr;
    }
    return error;
}
#endif
