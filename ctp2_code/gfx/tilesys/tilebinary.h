#ifndef CTP2_TILEBINARY_H
#define CTP2_TILEBINARY_H
#include <cstring>
#include <type_traits>

// Tile files pack 16- and 32-bit fields consecutively. Their file offsets do
// not guarantee native alignment; memcpy preserves the existing byte format.
template<class T> inline T tile_ReadScalar(const void *data)
{
    static_assert(std::is_integral<T>::value, "Tile scalar must be integral");
    T value;
    std::memcpy(&value, data, sizeof(value));
    return value;
}
#endif
