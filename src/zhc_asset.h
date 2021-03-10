#ifndef ZHC_ASSET_H
#define ZHC_ASSET_H

#include "lib/stb_truetype.h"
#include "lib/stb_image.h"

// NOTE(dgl): overengineered asset system, to learn how to create a general purpose allocator

typedef int32 Asset_ID;

enum Asset_Type
{
    Asset_Type_Text,
    Asset_Type_Font,
    Asset_Type_Image
};

// NOTE(dgl): free memory blocks
struct Asset_Memory_Block
{
    Asset_Memory_Block *prev;
    Asset_Memory_Block *next;
    bool32 used;
    usize size;
};

struct Loaded_Font
{
    uint8 *ttf_buffer;
    stbtt_fontinfo stbfont;

    int32 glyph_count;
    stbtt_bakedchar *glyphs;
};

struct Loaded_Image
{
    int32 width;
    int32 height;
    uint32 *pixels;
};

struct Loaded_Text
{
    usize size;
    uint8 *memory;
};

// NOTE(dgl): used memory blocks
struct Asset_Memory_Header
{
    Asset_Memory_Header *prev;
    Asset_Memory_Header *next;
    Asset_Type type;
    Asset_ID index;
    usize total_size;
    union
    {
        Loaded_Font font;
        Loaded_Image image;
        Loaded_Text text;
    };
};

struct Asset
{
    Asset_Memory_Header *header;
};

struct Zhc_Assets
{
    // NOTE(dgl): this arena is only used to allocate the asset files and assets.
    // the data allocation of the files uses the memory.
    DGL_Mem_Arena *permanent_arena;

    uint8 *memory;

    Asset_Memory_Header header_sentinel;
    Asset_Memory_Block memory_sentinel;

    int32 asset_count;
    // NOTE(dgl): shuold be the last allocation in the arena.
    // Then we can use it as dynamic array without much overhead.
    Asset *assets;
};

#endif // ZHC_ASSET_H
