#ifndef ZHC_ASSET_H
#define ZHC_ASSET_H

#include "lib/stb_truetype.h"
#include "lib/stb_image.h"

// NOTE(dgl): overengineered asset system, to learn how to create a general purpose allocator

typedef int32 Asset_ID;

enum Asset_Type
{
    Asset_Type_Data,
    Asset_Type_Font,
    Asset_Type_Image
};

enum Asset_Tag
{
    Asset_Tag_Icon,
    Asset_Tag_Font_Meta,
    Asset_Tag_Font_Bitmap
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

    // NOTE(dgl): these are 0 when initialized
    // because the font is not baked yet. It must be checked
    // if the font is baked before using it!
    int32 size;
    real32 height;
    real32 linegap;
    // NOTE(dgl): the depending bitmap is currently tracked in the UI code
    // because we allocate the assets there. The bitmap id should move here
    // eventually! @cleanup

    int32 glyph_count;
    stbtt_bakedchar *glyphs;
};

struct Loaded_Image
{
    int32 width;
    int32 height;
    uint32 *pixels;
};

struct Loaded_Data
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
        Loaded_Data data;
    };
};

// TODO(dgl): It would be great if all assets could have a dependent
// asset. Not only the files. But then we must restructure our asset
// allocation system. I think it would be better to have a asset
// file which describes these relations
struct Asset
{
    Asset_ID file_index;
    Asset_Tag tag;
    Asset_Memory_Header *header;
};

struct Asset_File
{
    Asset_ID asset;
    Zhc_File_Handle handle;
    usize size;
};

struct Zhc_Assets
{
    // NOTE(dgl): this arena is only used to allocate the asset files and assets.
    // the data allocation of the files uses the memory.
    DGL_Mem_Arena *permanent_arena;

    uint8 *memory;

    Asset_Memory_Header header_sentinel;
    Asset_Memory_Block memory_sentinel;

    // TODO(dgl): limit amout of assets and files

    int32 file_count;
    Asset_File *files;

    int32 asset_count;
    // NOTE(dgl): shuold be the last allocation in the arena.
    // Then we can use it as dynamic array without much overhead.
    Asset *assets;
};

internal Zhc_Assets *assets_begin_allocate(DGL_Mem_Arena *permanent_arena, usize memory_size);
internal void assets_end_allocate(Zhc_Assets *assets);
internal Asset_ID assets_push_file(Zhc_Assets *assets, Zhc_File_Handle handle, usize size);
internal Asset_ID assets_push(Zhc_Assets *assets);

// NOTE(dgl): simply allocate the memory
internal void assets_allocate_font(Zhc_Assets *assets, Asset_ID index, usize ttf_size);
internal void assets_allocate_image(Zhc_Assets *assets, Asset_ID index, int32 width, int32 height);
internal void assets_allocate_data(Zhc_Assets *assets, Asset_ID index, usize size);

// NOTE(dgl): allocate the memory and load (+parse) the data from the file handle
internal void assets_load_font(Zhc_Assets *assets, Asset_ID index);
internal void assets_load_image(Zhc_Assets *assets, Asset_ID index);

internal void assets_unload(Zhc_Assets *assets, Asset_ID index);

#define assets_get_font(assets, index) cast(Loaded_Font *) assets_get_(assets, index, Asset_Type_Font)
#define assets_get_image(assets, index) cast(Loaded_Image *) assets_get_(assets, index, Asset_Type_Image)
#define assets_get_data(assets, index) cast(Loaded_Data *) assets_get_(assets, index, Asset_Type_Data)
internal void *assets_get_(Zhc_Assets *assets, Asset_ID index, Asset_Type type);
#endif // ZHC_ASSET_H

