internal Asset_Memory_Block *
insert_block(Asset_Memory_Block *prev, uint8 *memory, usize size)
{
    assert(size > sizeof(Asset_Memory_Block), "Memory size must be larger than block size");
    Asset_Memory_Block *block = cast(Asset_Memory_Block *)memory;
    block->size = size - sizeof(Asset_Memory_Block);
    block->used = false;
    block->prev = prev;
    block->next = prev->next;
    block->prev->next = block;
    block->next->prev = block;

    return(block);
}

internal bool32
maybe_merge(Zhc_Assets *assets, Asset_Memory_Block *first, Asset_Memory_Block *second)
{
    bool32 result = false;
    if((first != &assets->memory_sentinel) &&
       (second != &assets->memory_sentinel))
    {
        if(!first->used && !second->used)
        {
            // NOTE(dgl): check if the blocks are adjacent
            uint8 *expected = (uint8 *)first + sizeof(Asset_Memory_Block) + first->size;
            if(cast(uint8 *)second == expected)
            {
                second->next->prev = second->prev;
                second->prev->next = second->next;

                first->size += sizeof(Asset_Memory_Block) + second->size;

                result = true;
            }
        }
    }
    return(result);
}

internal void
insert_header(Zhc_Assets *assets, Asset_Memory_Header *header)
{
    Asset_Memory_Header *sentinel = &assets->header_sentinel;

    header->prev = sentinel;
    header->next = sentinel->next;
    header->next->prev = header;
    header->prev->next = header;
}

// NOTE(dgl): size is total required size (memory_header, data)
internal Asset_Memory_Header *
asset_acquire_memory(Zhc_Assets *assets, usize size, int32 asset_index, Asset_Type type)
{
    Asset_Memory_Header *result = 0;

    Asset_Memory_Block *block = 0;
    for(block = assets->memory_sentinel.next;
        block != &assets->memory_sentinel;
        block = block->next)
    {
        if(!block->used && block->size >= size)
        {
            break;
        }
    }

    assert(block, "No free asset block found");
    assert(block->size >= size, "Asset does not fit");

    block->used = true;
    result = (Asset_Memory_Header *)(block + 1); /* start memory header after block header */
    int32 remaining = dgl_safe_size_to_int32(block->size - size);
    if(remaining > 4096)
    {
        block->size -= cast(usize)remaining;
        insert_block(block, (uint8 *)result + size, cast(usize)remaining);
    }

    if(result)
    {
        memset(result, 0, size);
        result->type = type;
        result->index = asset_index;
        result->total_size = size;
    }

    return(result);
}

internal void
asset_release_memory(Zhc_Assets *assets, Asset *asset)
{
    Asset_Memory_Header *header = asset->header;
    Asset_Memory_Block *block = (Asset_Memory_Block *)header - 1;
    block->used = false;
    maybe_merge(assets, block, block->next);
    maybe_merge(assets, block->prev, block);

    asset->header = 0;
}

// NOTE(dgl): It is easy to extend this function and load and unload assets automatically,
// by using a separate structure which holds the file info and a handle. This file info
// must be known when we allocate the asset memory. With this system it is possible to load
// the data in multiple threads.
// This is currently not needed and difficult because we have some assets loaded from from
// a buffer (e.g. the font bitmap).
internal void
assets_load_font(Zhc_Assets *assets, Asset_ID index, uint8 *ttf_buffer, usize buffer_size)
{
    assert(assets->assets, "Call assets_end_allocate before loading assets");
    Asset *asset = assets->assets + index;

    if(!asset->header)
    {
        // NOTE(dgl): we only encode ASCII + Latin-1 (first 256 code points)
        // if we need more, use glyphsets with each 256 characters to reduce
        // the amount of memory needed.
        int32 glyph_count = 256;
        usize total_size = buffer_size +
                           (sizeof(stbtt_bakedchar) * cast(usize)glyph_count) +
                           sizeof(Asset_Memory_Header);

       asset->header = asset_acquire_memory(assets, total_size, index, Asset_Type_Font);
       Loaded_Font *font = &asset->header->font;

       font->glyph_count = glyph_count;
       font->glyphs = cast(stbtt_bakedchar *)(asset->header + 1);
       font->ttf_buffer = cast(uint8 *)(font->glyphs + font->glyph_count);

       dgl_memcpy(font->ttf_buffer, ttf_buffer, buffer_size);

       int32 init_success = stbtt_InitFont(&font->stbfont, font->ttf_buffer, stbtt_GetFontOffsetForIndex(font->ttf_buffer,0));
       assert(init_success, "Failed to load font");
       LOG_DEBUG("Font - ttf_buffer: %p, data: %p, user_data: %p, font_start: %d, hhea: %d", font->ttf_buffer, font->stbfont.data, font->stbfont.userdata, font->stbfont.fontstart, font->stbfont.hhea);
   }
}

// NOTE(dgl): we use a uint8 buffer here to make passing file data easier.
internal void
assets_load_image(Zhc_Assets *assets, Asset_ID index, uint8 *buffer, usize buffer_size, int32 width, int32 height)
{
    assert(assets->assets, "Call assets_end_allocate before loading assets");
    Asset *asset = assets->assets + index;
    if(!asset->header)
    {
        usize bitmap_size = sizeof(uint32) * cast(usize)(width * height);
        usize total_size = bitmap_size +
                           sizeof(Asset_Memory_Header);

        asset->header = asset_acquire_memory(assets, total_size, index, Asset_Type_Image);
        Loaded_Image *image = &asset->header->image;

        image->width = width;
        image->height = height;
        image->pixels = cast(uint32 *)(asset->header + 1);

        assert(buffer_size <= bitmap_size, "Image buffer cannot be bigger than the bitmap");

        dgl_memcpy((uint8 *)image->pixels, buffer, buffer_size);
    }
}

internal void
assets_load_text(Zhc_Assets *assets, Asset_ID index, uint8 *buffer, usize buffer_size)
{
    assert(assets->assets, "Call assets_end_allocate before loading assets");
    Asset *asset = assets->assets + index;
    if(!asset->header)
    {
        usize total_size = buffer_size +
                           sizeof(Asset_Memory_Header);

        asset->header = asset_acquire_memory(assets, total_size, index, Asset_Type_Text);
        Loaded_Text *text = &asset->header->text;

        text->size = buffer_size;
        text->memory = cast(uint8 *)(asset->header + 1);
        dgl_memcpy(text->memory, buffer, buffer_size);
    }
}

#define assets_get_font(assets, index) cast(Loaded_Font *) assets_get_(assets, index, Asset_Type_Font)
#define assets_get_image(assets, index) cast(Loaded_Image *) assets_get_(assets, index, Asset_Type_Image)
#define assets_get_text(assets, index) cast(Loaded_Text *) assets_get_(assets, index, Asset_Type_Text)
internal void *
assets_get_(Zhc_Assets *assets, Asset_ID index, Asset_Type type)
{
    void *result = 0;

    Asset *asset = assets->assets + index;
    if(asset->header)
    {
        assert(asset->header->type == type, "Type does not match asset type");
        if(type == Asset_Type_Font) { result = &asset->header->font; }
        else if(type == Asset_Type_Image) { result = &asset->header->image; }
        else if(type == Asset_Type_Text) { result = &asset->header->text; }
        else
        {
            LOG_DEBUG("Unsupported asset type %d", type);
        }
    }
    else
    {
        LOG("Asset %d currently not loaded", index);
    }
    return(result);
}

internal void
assets_unload(Zhc_Assets *assets, Asset_ID index)
{
    Asset *asset = assets->assets + index;
    if(asset->header)
    {
        asset_release_memory(assets, asset);
    }
}

internal Asset_ID
assets_push(Zhc_Assets *assets)
{
    Asset_ID result = 0;
    result = assets->asset_count++;
    return(result);
}

// NOTE(dgl): file if is the same as the list element in group
// TODO(dgl): Find better way to identify files.
internal Zhc_Assets *
assets_begin_allocate(DGL_Mem_Arena *permanent_arena, usize size)
{
    Zhc_Assets *result = dgl_mem_arena_push_struct(permanent_arena, Zhc_Assets);
    result->permanent_arena = permanent_arena;

    result->memory = dgl_mem_arena_push_array(result->permanent_arena, uint8, size);

    result->memory_sentinel.next = &result->memory_sentinel;
    result->memory_sentinel.prev = &result->memory_sentinel;
    result->header_sentinel.next = &result->header_sentinel;
    result->header_sentinel.prev = &result->header_sentinel;

    insert_block(&result->memory_sentinel, result->memory, size);

    result->asset_count = 0;

    return(result);
}

internal void
assets_end_allocate(Zhc_Assets *assets)
{
    // NOTE(dgl): put file ids into assets. If assets do not have a file id, they are child assets
    // of others (like the font bitmap).
    assets->assets = dgl_mem_arena_push_array(assets->permanent_arena, Asset, cast(usize)assets->asset_count);
}

// internal void
// load_png(Zhc_File_Group *assets, char *name)
// {
//     int32 x;
//     int32 y;
//     int32 n;

//     uint8 *data = stbi_load(filename, &x, &y, &n, 4);
//     // TODO(dgl): copy into own arena
//     stbi_image_free(data)
// }
