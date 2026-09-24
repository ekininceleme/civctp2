#ifndef SLIC_BYTECODE_H
#define SLIC_BYTECODE_H

#include <cstring>
#include <type_traits>

// Operands follow single-byte opcodes and need not be naturally aligned.
// Copy values to preserve the existing native bytecode layout without typed
// pointer dereferences into the byte stream.
namespace SlicBytecode {
template<class T> T Read(const void *address)
{
    static_assert(std::is_trivially_copyable<T>::value, "bytecode operands must be trivially copyable");
    T value;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

template<class T> void Write(void *address, T value)
{
    static_assert(std::is_trivially_copyable<T>::value, "bytecode operands must be trivially copyable");
    std::memcpy(address, &value, sizeof(value));
}
}
#endif
