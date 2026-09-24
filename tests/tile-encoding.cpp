#include "c3.h"
#include "pixelutils.h"
#include "tileutils.h"
#include <cassert>
#include <algorithm>
#include <initializer_list>
#include <memory>
#include <vector>

sint32 g_is565Format = 0;

char encode(Pixel16 *input, int width, Pixel16 **output, bool source565)
{
    return tileutils_EncodeScanline16(input, width, output, source565);
}
char encode(Pixel32 *input, int width, Pixel16 **output, bool)
{
    return tileutils_EncodeScanline(input, width, output);
}

// Exact-size heap buffers let ASan detect even a single lookahead past a row.
template<class Pixel>
void check(std::initializer_list<Pixel> pixels, std::initializer_list<Pixel16> expected,
           bool empty, bool source565 = false)
{
    std::unique_ptr<Pixel[]> input(new Pixel[pixels.size()]);
    std::copy(pixels.begin(), pixels.end(), input.get());
    std::vector<Pixel16> output(pixels.size() * 2 + 1, 0xffff);
    auto end = output.data();
    char result = encode(input.get(), pixels.size(), &end, source565);
    assert(bool(result) == empty);
    assert(end - output.data() == expected.size());
    assert(std::equal(expected.begin(), expected.end(), output.begin()));
    assert(*end == 0xffff);
}

int main()
{
    check<Pixel16>({0}, {}, true);
    check<Pixel16>({0x1234}, {0xec01, 0x1234}, false);
    check<Pixel16>({0x7c1f}, {0xed01}, false);
    check<Pixel16>({0x03ff}, {0xeb01}, false);
    check<Pixel16>({0x1234, 0x1234}, {0xec02, 0x1234, 0x1234}, false);
    check<Pixel16>({0, 0x7c1f, 0x03ff, 0x1234, 0},
                   {0x0a01, 0x0d01, 0x0b01, 0x0c01, 0x1234, 0xfa01}, false);
    check<Pixel16>({0xf81f}, {0xed01}, false, true);
    check<Pixel16>({0x07ff}, {0xeb01}, false, true);
    check<Pixel16>({0xffff}, {0xec01, 0x7fff}, false, true);
    check<Pixel16>({0}, {}, true, true);
    check<Pixel32>({0}, {}, true);
    check<Pixel32>({0xffffff}, {0xec01, 0x7fff}, false);
    check<Pixel32>({0xff00ff}, {0xed01}, false);
    check<Pixel32>({0xffff00}, {0xeb01}, false);
    check<Pixel32>({0xffffff, 0xffffff}, {0xec02, 0x7fff, 0x7fff}, false);
    check<Pixel32>({0, 0xff00ff, 0xffff00, 0xffffff, 0},
                   {0x0a01, 0x0d01, 0x0b01, 0x0c01, 0x7fff, 0xfa01}, false);
}
