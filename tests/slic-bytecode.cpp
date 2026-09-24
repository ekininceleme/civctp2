#include "../ctp2_code/gs/slic/SlicBytecode.h"
#include <cassert>
#include <cstdint>
#include <cstring>

int main()
{
    // Check every alignment, including the one-byte opcode prefix used by SLIC.
    alignas(16) unsigned char bytes[64];
    int pointee = 42;
    for (unsigned offset = 0; offset < 16; ++offset) {
        std::memset(bytes, 0xA5, sizeof(bytes));
        unsigned char *operand = bytes + offset;
        const int32_t integer = -1234567;
        const double real = 1.25;
        int *pointer = &pointee;
        SlicBytecode::Write<int32_t>(operand, integer);
        SlicBytecode::Write<double>(operand + sizeof(integer), real);
        SlicBytecode::Write<int *>(operand + sizeof(integer) + sizeof(real), pointer);
        assert(std::memcmp(operand, &integer, sizeof(integer)) == 0);
        assert(std::memcmp(operand + sizeof(integer), &real, sizeof(real)) == 0);
        assert(std::memcmp(operand + sizeof(integer) + sizeof(real), &pointer, sizeof(pointer)) == 0);
        assert(SlicBytecode::Read<int32_t>(operand) == integer);
        assert(SlicBytecode::Read<double>(operand + sizeof(integer)) == real);
        assert(SlicBytecode::Read<int *>(operand + sizeof(integer) + sizeof(real)) == pointer);
        SlicBytecode::Write<int *>(operand, nullptr);
        assert(SlicBytecode::Read<int *>(operand) == nullptr);
        for (unsigned i = 0; i < offset; ++i) assert(bytes[i] == 0xA5);
        for (unsigned i = offset + sizeof(integer) + sizeof(real) + sizeof(pointer); i < sizeof(bytes); ++i)
            assert(bytes[i] == 0xA5);
    }
}
