#include "../ctp2_code/gfx/tilesys/tilebinary.h"
#include <cassert>
#include <cstdint>
#include <cstring>
int main()
{
    alignas(8) unsigned char packed[16] = {};
    const std::uint16_t count = 17;
    const std::uint32_t length = 0x10203040;
    const std::int16_t negative = -123;
    for (unsigned offset = 0; offset < 8; ++offset) {
        std::memcpy(packed + offset, &count, sizeof(count));
        std::memcpy(packed + offset + 2, &length, sizeof(length));
        std::memcpy(packed + offset + 6, &negative, sizeof(negative));
        assert(tile_ReadScalar<std::uint16_t>(packed + offset) == count);
        assert(tile_ReadScalar<std::uint32_t>(packed + offset + 2) == length);
        assert(tile_ReadScalar<std::int16_t>(packed + offset + 6) == negative);
    }
}
