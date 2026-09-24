// FreeType 2 rasterization for the existing AUI glyph cache and text layout.
#include "c3.h"
#include "aui_bitmapfont.h"
#include "freetype_render.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>

#ifdef CTP2_FREETYPE2
aui_BitmapFont::GlyphInfo *aui_BitmapFont::CacheGlyph(uint16 codepoint)
{
    GlyphInfo *gi = &m_glyphs[codepoint];
    if (gi->surface) return gi;
    if (!m_ftFace || FT_Load_Char(m_ftFace, codepoint, FT_LOAD_NO_BITMAP)) return nullptr;
    ctp2_FontBitmap bitmap;
    if (!ctp2_RenderFontGlyph(m_ftFace->glyph, bitmap)) return nullptr;
    const sint32 width = bitmap.width;
    const sint32 rows = bitmap.height;
    const sint32 top = m_baseLine - bitmap.top;
    // A separate cache page can accommodate taller non-Latin glyphs without
    // changing the line metrics or clipping their actual bitmap.
    const sint32 cacheTop = std::max<sint32>(0, top);
    if (width >= k_AUI_BITMAPFONT_SURFACEWIDTH) return nullptr;
    aui_Surface *cache = m_surfaceList->L() ? m_surfaceList->GetTail() : nullptr;
    if (!cache || m_curOffset + width + 1 > cache->Width() || cacheTop + rows > cache->Height()) {
        AUI_ERRCODE error;
        cache = new aui_Surface(&error, k_AUI_BITMAPFONT_SURFACEWIDTH,
                               std::max(m_maxHeight, cacheTop + rows), 8);
        if (!AUI_SUCCESS(error)) { delete cache; return nullptr; }
        m_surfaceList->AddTail(cache);
        m_curOffset = 0;
    }
    if (width && rows) {
        RECT rect = {m_curOffset, cacheTop, m_curOffset + width, cacheTop + rows};
        void *pixels = nullptr;
        if (!AUI_SUCCESS(cache->Lock(&rect, &pixels, 0))) return nullptr;
        for (int row = 0; row < rows; ++row)
            std::memcpy(static_cast<unsigned char *>(pixels) + row * cache->Pitch(),
                        bitmap.pixels.data() + row * width, width);
        const AUI_ERRCODE unlocked = cache->Unlock(pixels);
        if (!AUI_SUCCESS(unlocked)) return nullptr;
    }
    gi->c = static_cast<MBCHAR>(codepoint);
#if defined(_JAPANESE)
    gi->c2 = codepoint;
#endif
    gi->bearingX = static_cast<sint16>(bitmap.bearingX);
    gi->bearingY = static_cast<sint16>(bitmap.bearingY);
    gi->advance = static_cast<sint16>(bitmap.advance);
    SetRect(&gi->bbox, m_curOffset, cacheTop, m_curOffset + width, cacheTop + rows);
    gi->surface = cache; // Publish only after the bitmap was copied successfully.
    m_curOffset += width + 1;
    return gi;
}

aui_BitmapFont::GlyphInfo *aui_BitmapFont::GetGlyphInfo(MBCHAR c)
{
    if (c != '\t') return CacheGlyph(static_cast<uint8>(c));
    GlyphInfo *tab = &m_glyphs[static_cast<uint8>(c)];
    if (!tab->surface) {
        GlyphInfo *space = CacheGlyph(' ');
        if (!space) return nullptr;
        *tab = *space;
        tab->c = '\t';
        tab->advance *= 4;
        if (m_tabSkip < 0) m_tabSkip = tab->advance;
    }
    return tab;
}

#if defined(_JAPANESE)
aui_BitmapFont::GlyphInfo *aui_BitmapFont::GetGlyphInfo(const MBCHAR *text)
{
    // Preserve the legacy locale-based decoding; Unicode text shaping is separate.
    wchar_t codepoint = 0;
    if (std::mbtowc(&codepoint, text, MB_CUR_MAX) < 1 || codepoint < 0 || codepoint > 65535)
        return GetGlyphInfo(*text);
    return CacheGlyph(static_cast<uint16>(codepoint));
}
#endif
#endif
