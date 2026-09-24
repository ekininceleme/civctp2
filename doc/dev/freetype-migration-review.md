# FreeType migration compatibility review

Reviewed 2026-09-24 against the official FreeType documentation and this checkout's FreeType 1.3.1 renderer. This records the contracts to preserve; it does not claim the current FreeType 2 renderer is visually equivalent.

## Is there a migration guide?

The official material reviewed includes the tutorial, glyph conventions, API reference, FAQ, and the v40 hinting transition article. I did not find a dedicated FreeType 1-to-2 guide promising visual compatibility. The [v40 article](https://freetype.org/freetype2/docs/hinting/subpixel-hinting.html) is useful for the later change in TrueType hinting, but is not a FreeType 1 migration guide. In particular, it explains why legacy Arial and Times New Roman can look different when horizontal hints are ignored.

## Contracts and likely regressions

| Contract | Legacy implementation and current risk | Compatibility approach |
| --- | --- | --- |
| Font identity | The face/style comes from the actual font file, not the library version. A changed file, charmap, or point size can masquerade as a rasterizer change. | Keep the same eight bundled font files, face index, character mapping and requested size; record them in comparison output. |
| Size and DPI | Both paths request points at 96 DPI. Points are not pixels: 12pt at 96 DPI corresponds to 16 pixels per EM, not necessarily a 16-pixel bitmap. | Preserve `FT_Set_Char_Size(..., pointSize * 64, 96, 96)`. Do not compensate for a rendering difference by changing DPI or font size. |
| Hinting and advances | `freetype_library.h` now selects interpreter 35. This addresses one documented difference from modern defaults, but cannot guarantee identical rasterization. | Explicitly select the property and handle failure. Compare advances, bearings, outlines and pixels independently. |
| Baseline and line height | Legacy `SetPointSize` aggregates hinted metrics for codes 0–255. The initial new path aggregated rendered bitmap extents. These are different definitions. | Compare the complete per-font baseline, maximum height and line skip; preserve the legacy calculation if they differ. Do not substitute global face ascender/descender without evidence. |
| Horizontal raster origin | Legacy `GetGlyphInfo` translates each outline by **negative exact `bbox.xMin`** before rendering into the cache. The new path renders at the default origin and uses `bitmap_left`. A fractional old xMin therefore implies different pixel coverage, even if advances match. | Compare the hinted control boxes and translations first. If preserving the old origin, translate the loaded outline before rendering and retain the original placement metrics separately. |
| Coverage and weight | Old initialization sets palette **0, 86, 128, 170, 255**. FreeType 2 NORMAL returns 256-level coverage. The old rasterizer counts a 2×2 sample cell and has dropout handling; mapping modern coverage to five values is not an exact reconstruction. | Measure coverage sums, per-pixel differences and final RGB output. A palette transfer can restore some visual weight, but must be labelled an approximation unless comparison proves otherwise. |
| Blending | Existing `RenderGlyph16` treats cache bytes as opacity, special-cases 255, and passes `coverage >> 3` to RGB555/565 blend functions. This makes palette 86 an effective 10/32 blend, 128 → 16/32, 170 → 21/32. | Preserve this compositor during compatibility work. Test actual 16-bit output, not only grayscale previews. Do not add arbitrary gamma or emboldening as a substitute for identifying the changed contract. |
| Cache and placement | FT2 slot contents are replaced when another glyph loads. Bitmap width is independent of advance; italic overhang and whitespace are especially important. | Copy pixels before the next load, keep advance separate from bitmap dimensions, preserve negative bearings, accept empty glyphs, and test signed pitch. |
| Layout and scaling | AUI owns wrapping, clipping, tabs, underline, shadow and SDL logical scaling. FreeType does not implement the game's layout rules. | Compare complete strings through AUI at identical logical size and display scale after isolated glyph tests pass. |

Sizing, face identity, charmap selection, slot lifetime, bitmap bearings and outline-versus-bitmap loading are documented in the [official loading tutorial](https://freetype.org/freetype2/docs/tutorial/step1.html). The [glyph metrics conventions](https://freetype.org/freetype2/docs/glyphs/glyphs-3.html) distinguish bearings, advance, line spacing and the effects of grid fitting. The [driver-property reference](https://freetype.org/freetype2/docs/reference/ft2-properties.html#interpreter-version) identifies interpreter 35 as the legacy grayscale/monochrome engine; it does not promise FreeType 1 pixel identity.

The [outline API](https://freetype.org/freetype2/docs/reference/ft2-outline_processing.html#ft_outline_translate) provides explicit translation and explains that a control box can include Bézier control points outside the exact curve bounds. Use the same kind of bounds when comparing old and new calculations. [Bitmap conventions](https://freetype.org/freetype2/docs/glyphs/glyphs-7.html) explain row orientation, pitch and gray levels.

The [glyph-rendering reference](https://freetype.org/freetype2/docs/reference/ft2-glyph_retrieval.html#ft_render_glyph) defines grayscale values as alpha coverage and recommends blending in linear color space with gamma conversion. That is guidance for a modern compositor, **not evidence that changing the game's existing compositor preserves its appearance**. The [rendering article](https://freetype.org/freetype2/docs/hinting/text-rendering-general.html) also distinguishes gamma correction from stem darkening; arbitrary darkening changes outlines and can hide the real regression.

Local primary evidence: [`aui_bitmapfont.cpp`](../../ctp2_code/ui/aui_common/aui_bitmapfont.cpp) (`InitCommon`, `SetPointSize`, legacy `GetGlyphInfo`, `RenderGlyph16`), [`aui_bitmapfont_freetype2.cpp`](../../ctp2_code/ui/aui_common/aui_bitmapfont_freetype2.cpp), [`aui_pixel.h`](../../ctp2_code/ui/aui_common/aui_pixel.h), and bundled FreeType 1 [`ttraster.c`](../../ctp2_code/libs/freetype-1.3.1/lib/ttraster.c) (`count_table`, `Vertical_Gray_Sweep_Step`, `Horizontal_Gray_Sweep_Drop`). The [official FAQ](https://freetype.org/freetype2/docs/ft2faq.html) describes the modern rasterizer's area coverage algorithm and warns that build options can change rendering across platforms.

## Acceptance evidence

Keep legacy output as the oracle rather than producing golden images only from the new implementation. For every bundled face and representative UI size, compare advances, bearings, baseline, line height, pixel bounds, cache images and coverage totals. Include accents, descenders, italic overhang, punctuation, space, missing glyphs and tabs. Save aligned old/new/difference images and compare final RGB555/565 pixels on both dark and light backgrounds; average darkness alone can hide shifted or clipped strokes. Then exercise AUI wrapping, truncation, underline, shadow and text-field cursor placement, with 100% and the user's display scale held constant between builds.

Exact pixel equality is a stronger requirement than equivalent typography. If modern rasterization cannot reproduce old coverage, report the measured residual difference explicitly rather than calling the migration identical. Pin the tested library version and interpreter configuration in results; cross-platform builds need their own rendering evidence because FreeType configuration can differ.

## Compatibility rasterizer experiment

A promising approximation is to load and hint at the original size using interpreter 35, normalize the outline origin as the old code does, scale that **already hinted outline** by two, render monochrome, and reduce each 2×2 block through the old five-entry palette. Do not load the font at twice the point size: hinting at twice the size produces different outlines. The [tutorial's transformation section](https://freetype.org/freetype2/docs/tutorial/step1.html#section-6) explicitly distinguishes scaling an already hinted glyph from loading at another size.

Select monochrome raster output without silently changing the load-time hinting target. `FT_RENDER_MODE_MONO` and `FT_LOAD_TARGET_MONO` serve different stages; the latter can change hinting. Preserve the original glyph's advance and placement metrics separately from the transformed raster. Check empty glyphs, negative bearings, odd bitmap dimensions, pitch padding, clipping, and integer overflow in doubled dimensions. Legacy grayscale dropout corrections and modern monochrome raster behavior can still differ, so this approach needs measured residual-error reporting rather than an exact-compatibility claim. These distinctions follow from the [glyph loading and render-mode reference](https://freetype.org/freetype2/docs/reference/ft2-glyph_retrieval.html).

## Implemented and measured in this checkout

The CMake font path now fixes interpreter 35, preserves 96 DPI, restores
metrics-based line height, rounds hinted control boxes outward as FT1 did,
normalizes the raster origin, and uses smooth 256-level grayscale rendering,
which the user explicitly preferred after comparing the five-level experiment. RGB555/565
compositing, text layout, tab logic and scaling remain in the existing AUI code.
No synthetic bolding or gamma adjustment was added.

`tools/cmake/freetype1-reference.c` builds against the original bundled library
and supplies the independent oracle in `tests/freetype-legacy-raster.h`.
Across eight bundled faces, 8/10/12/16pt and printable ASCII:

- All 3,040 advances and all 32 baseline/line-height pairs match exactly.
- Residual bounds/bearing differences are at most one pixel; worst case is
  15 differing values out of 475 for one font/size, primarily italic glyphs.
- Total glyph opacity differs from the old renderer by -3.090% to +0.161%.
  This is a coverage measurement, not a perceptual-equivalence guarantee.
- Tests require exact advances and line metrics, at most one pixel for other
  metrics (maximum 20 per face/size), and opacity within 4% of the old output. Smooth intermediate coverage levels
  are required so a return to five-level antialiasing fails the test.
- The compatibility renderer also exercises codes 0..255 at 8/12/18/32pt,
  including empty glyphs, accents and missing-glyph fallback, under sanitizers.

The original rasterizer's dropout rules are not identical to modern FreeType's.
Consequently this preserves typography while intentionally smoothing the edges but does not
promise pixel-identical glyphs. The remaining per-pixel differences, and full
in-game wrapping/clipping across every screen, are not covered by exact golden
image tests. Desktop automated mouse input is currently unreliable with this
SDL configuration; the New Game render/shutdown test is driven in-process.
