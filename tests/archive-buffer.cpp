#include "c3.h"
#include "civarchive.h"
#include <cassert>
#include <vector>

int main()
{
    std::vector<uint8> input(20000), output(input.size());
    for (size_t i = 0; i < input.size(); ++i) input[i] = static_cast<uint8>(i);
    CivArchive archive;
    archive.Store(input.data(), input.size()); // force growth beyond the initial allocation
    assert(archive.StreamLen() == input.size());
    archive.ResetForLoad();
    archive.Load(output.data(), output.size());
    assert(input == output);
    archive.SetSize(32); // replace an existing allocation
    archive.SetStore();
    archive.ResetForLoad();
    // Scope exit also checks the destructor's matching allocation/deallocation.
}
