#include "../ctp2_code/ui/aui_common/freetype_library.h"
#include "freetype-legacy-raster.h"
#include "../ctp2_code/ui/aui_common/freetype_render.h"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>

int main(int argc, char **argv)
{
    assert(argc > 1);
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    assert(ctp2_InitFontLibrary(&library) == 0);
    for (int font = 1; font < argc; ++font) {
    assert(FT_New_Face(library, argv[font], 0, &face) == 0);
    assert(FT_Select_Charmap(face, FT_ENCODING_UNICODE) == 0);
    for (int size : {8, 12, 18, 32}) {
        assert(FT_Set_Char_Size(face, 0, size * 64, 96, 96) == 0);
        // Font loading measures every Latin-1 glyph, including control codes.
        for (unsigned c = 0; c < 256; ++c) {
            assert(FT_Load_Char(face, c, FT_LOAD_NO_BITMAP) == 0);
            ctp2_FontBitmap compatibilityGlyph;
            assert(ctp2_RenderFontGlyph(face->glyph, compatibilityGlyph));
            assert(compatibilityGlyph.pixels.size() ==
                   static_cast<std::size_t>(compatibilityGlyph.width) * compatibilityGlyph.height);
            if (c == 32) assert(compatibilityGlyph.pixels.empty());
        }
    }
    FT_Done_Face(face);
    }
    for (const auto &reference : legacyFontRasters) {
        const char *path = nullptr;
        for (int font = 1; font < argc; ++font) {
            const char *name = std::strrchr(argv[font], '/');
            if (!name) name = std::strrchr(argv[font], '\\');
            name = name ? name + 1 : argv[font];
            if (std::strcmp(name, reference.font) == 0) path = argv[font];
        }
        assert(path);
        assert(FT_New_Face(library, path, 0, &face) == 0);
        assert(FT_Select_Charmap(face, FT_ENCODING_UNICODE) == 0);
        assert(FT_Set_Char_Size(face, 0, reference.size * 64, 96, 96) == 0);
        int baseline = 0, height = 0;
        assert(ctp2_GetLegacyFontMetrics(face, baseline, height));
        assert(baseline == reference.baseline && height == reference.height);
        unsigned opacity = 0;
        bool smoothCoverage = false;
        unsigned metricDifferences = 0;
        for (unsigned c = 32; c < 127; ++c) {
            assert(FT_Load_Char(face, c, FT_LOAD_NO_BITMAP) == 0);
            ctp2_FontBitmap glyph;
            assert(ctp2_RenderFontGlyph(face->glyph, glyph));
            const int actual[] = {glyph.width, glyph.height, glyph.bearingX, glyph.bearingY, glyph.advance};
            for (int metric = 0; metric < 5; ++metric) {
                const int difference = std::abs(actual[metric] - reference.metrics[c - 32][metric]);
                // FT1 and FT2 retain small outline-hinting differences, notably
                // italic overhang. Advances must match exactly; bounds/bearings
                // may differ by one pixel, in at most 20/475 metrics per face/size.
                assert(difference <= (metric == 4 ? 0 : 1));
                metricDifferences += difference != 0;
            }
            for (unsigned char value : glyph.pixels) {
                opacity += value;
                smoothCoverage |= value != 0 && value != 86 && value != 128 && value != 170 && value != 255;
            }
        }
        assert(metricDifferences <= 20);
        assert(smoothCoverage);
        const double ratio = static_cast<double>(opacity) / reference.opacity;
        std::printf("%s %dpt opacity change: %.3f%%\n", reference.font, reference.size, (ratio - 1) * 100);
        // Rasterizers retain different dropout behavior. Bound total ink rather
        // than claiming pixel equivalence. Advances and line metrics remain exact.
        // Smooth area coverage intentionally differs from the old five levels;
        // measured worst case is -3.09% for Times Bold Italic at 8pt.
        assert(ratio >= 0.96 && ratio <= 1.04);
        FT_Done_Face(face);
    }
    FT_Done_FreeType(library);
}
