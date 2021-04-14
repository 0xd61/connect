#ifndef ZHC_UTILS
#define ZHC_UTILS

/* 32bit fnv-1a hash */
#define HASH_OFFSET_BASIS 0x811C9DC5
#define HASH_PRIME 0x01000193

inline void
hash(uint32 *hash, void *data, usize data_count)
{
    uint8 *octet = (uint8 *)data;
    while(data_count)
    {
        *hash = (*hash ^ *octet) * HASH_PRIME;
        data_count--;
        octet++;
    }
}

inline uint32
parse_version(char *string)
{
    uint32 result = 0;
    int32 segment = 0;
    int32 number = 0;
    while(*string)
    {
        if(*string == '.')
        {
            // NOTE(dgl): major
            if(segment == 0)
            {
                assert(number <= 0xFFFF && number >= 0, "Invalid version segment (cannot be bigger than 65535)");
                result |= (cast(uint32)number << 16);
            }
            // NOTE(dgl): minor and patch
            else if(segment == 1)
            {
                assert(number <= 0xFF && number >= 0, "Invalid version segment (cannot be bigger than 255)");
                result |= (cast(uint32)number << 8);
            }

            ++segment;
            number = 0;
            ++string;
        }

        number *= 10;
        number += *string - '0';

        ++string;
    }
    assert(segment == 2, "Failed parsing the version number (too many segments)");
    assert(number <= 0xFF && number >= 0, "Invalid version segment (cannot be bigger than 255)");
    result |= cast(uint32)number;

    return(result);
}

//
// NOTE(dgl): Intrinsics
//

inline int32
bits_required(uint64 value)
{
    int32 result = 64 - __builtin_clzll(value);
    return(result);
}

inline uint16
bswap16(uint16 value)
{
    uint16 result = __builtin_bswap16(value);
    return(result);
}

inline uint32
bswap32(uint32 value)
{
    uint32 result = __builtin_bswap32(value);
    return(result);
}

inline uint64
bswap64(uint64 value)
{
    uint64 result = __builtin_bswap64(value);
    return(result);
}

#endif // ZHC_UTILS
