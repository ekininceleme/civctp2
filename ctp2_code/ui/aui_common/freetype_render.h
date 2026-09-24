#ifndef CTP2_FREETYPE_RENDER_H
#define CTP2_FREETYPE_RENDER_H
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include <vector>
#include <algorithm>

inline FT_BBox ctp2_LegacyFontBox(FT_GlyphSlot glyph)
{
    FT_BBox box;
    FT_Outline_Get_CBox(&glyph->outline, &box);
    // FreeType 1 grid-fits the metrics box outward after hinting.
    box.xMin &= ~static_cast<FT_Pos>(63);
    box.yMin &= ~static_cast<FT_Pos>(63);
    box.xMax = (box.xMax + 63) & ~static_cast<FT_Pos>(63);
    box.yMax = (box.yMax + 63) & ~static_cast<FT_Pos>(63);
    return box;
}

inline bool ctp2_GetLegacyFontMetrics(FT_Face face, int &baseline, int &height)
{
    FT_Pos ascend = 0, descend = 0;
    for (unsigned c = 0; c < 256; ++c) {
        if (FT_Load_Char(face, c, FT_LOAD_NO_BITMAP)) return false;
        const FT_BBox box = ctp2_LegacyFontBox(face->glyph);
        const FT_Pos bearing = face->glyph->metrics.horiBearingY;
        ascend = std::max(ascend, bearing);
        descend = std::max(descend, box.yMax - box.yMin - bearing);
    }
    baseline = static_cast<int>(ascend / 64);
    height = baseline + static_cast<int>(descend / 64);
    return true;
}

struct ctp2_FontBitmap {
    int width = 0, height = 0, bearingX = 0, bearingY = 0, advance = 0;
    int top = 0;
    std::vector<unsigned char> pixels;
};

// Preserve AUI placement metrics and raster origin while using modern smooth
// 256-level antialiasing. This consumes the slot outline; reload before reuse.
inline bool ctp2_RenderFontGlyph(FT_GlyphSlot glyph, ctp2_FontBitmap &out)
{
    if (glyph->format != FT_GLYPH_FORMAT_OUTLINE) return false;
    const FT_BBox box = ctp2_LegacyFontBox(glyph);
    const auto roundPixel = [](FT_Pos value) -> int {
        // The original uses floor(value / 64.0 + 0.5), including negatives.
        return static_cast<int>(value >= -32 ? (value + 32) / 64 : (value - 31) / 64);
    };
    out.width = roundPixel(box.xMax) - roundPixel(box.xMin);
    out.height = static_cast<int>(box.yMax / 64 - box.yMin / 64);
    out.top = static_cast<int>(box.yMax / 64);
    out.bearingX = roundPixel(glyph->metrics.horiBearingX);
    out.bearingY = static_cast<int>(-glyph->metrics.horiBearingY / 64);
    out.advance = roundPixel(glyph->advance.x);
    if (out.width < 0 || out.height < 0 || out.width > 4096 || out.height > 4096)
        return false;
    out.pixels.assign(static_cast<std::size_t>(out.width) * out.height, 0);
    if (!out.width || !out.height) return true;

    FT_Bitmap bitmap = {};
    bitmap.width = out.width;
    bitmap.rows = out.height;
    bitmap.pitch = out.width;
    bitmap.buffer = out.pixels.data();
    bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
    bitmap.num_grays = 256;
    FT_Outline_Translate(&glyph->outline, -box.xMin, -(box.yMin / 64) * 64);
    return FT_Outline_Get_Bitmap(glyph->library, &glyph->outline, &bitmap) == 0;
}
#endif
