
#include "lib/sodium.h"

internal void
get_random_bytes(uint8 *buffer, usize buffer_size)
{
    assert(buffer, "Buffer for random bytes must exist");
    randombytes_buf(buffer, buffer_size);
}
