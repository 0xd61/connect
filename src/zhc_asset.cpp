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

// NOTE(dgl): the allocate functions are needed for loading in memory data. This is an intermediate
// step until we have a better idea to load this kind of data into assets
internal void
assets_allocate_font(Zhc_Assets *assets, Asset_ID index, uint8 *ttf_buffer, usize buffer_size)
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

// TODO(dgl): do a prepare image
internal void
assets_allocate_image(Zhc_Assets *assets, Asset_ID index, uint8 *buffer, usize buffer_size, int32 width, int32 height)
{
    // NOTE(dgl): only supports png files
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
assets_allocate_data(Zhc_Assets *assets, Asset_ID index, uint8 *buffer, usize buffer_size)
{
    assert(assets->assets, "Call assets_end_allocate before loading assets");
    Asset *asset = assets->assets + index;
    if(!asset->header)
    {
        usize total_size = buffer_size +
                           sizeof(Asset_Memory_Header);

        asset->header = asset_acquire_memory(assets, total_size, index, Asset_Type_Data);
        Loaded_Data *data = &asset->header->data;

        data->size = buffer_size;
        data->memory = cast(uint8 *)(asset->header + 1);
        dgl_memcpy(data->memory, buffer, buffer_size);
    }
}

// TODO(dgl): support dynamic loading for images and data
internal void
assets_load_font(Zhc_Assets *assets, Asset_ID index)
{
    // NOTE(dgl): only supports .ttf files
    assert(assets->assets, "Call assets_end_allocate before loading assets");
    Asset *asset = assets->assets + index;

    if(!asset->header)
    {
        if(asset->file_index > 0)
        {
            Asset_File *file = assets->files + asset->file_index;

            // NOTE(dgl): we only encode ASCII + Latin-1 (first 256 code points)
            // if we need more, use glyphsets with each 256 characters to reduce
            // the amount of memory needed.
            int32 glyph_count = 256;
            usize total_size = file->size +
                               (sizeof(stbtt_bakedchar) * cast(usize)glyph_count) +
                               sizeof(Asset_Memory_Header);

            asset->header = asset_acquire_memory(assets, total_size, index, Asset_Type_Font);
            Loaded_Font *font = &asset->header->font;

            font->glyph_count = glyph_count;
            font->glyphs = cast(stbtt_bakedchar *)(asset->header + 1);
            font->ttf_buffer = cast(uint8 *)(font->glyphs + font->glyph_count);

            platform.read_entire_file(&file->handle, font->ttf_buffer, file->size);
            assert(file->handle.no_error, "Could not load font");

            int32 init_success = stbtt_InitFont(&font->stbfont, font->ttf_buffer, stbtt_GetFontOffsetForIndex(font->ttf_buffer,0));
            assert(init_success, "Failed to load font");
            LOG_DEBUG("Font - ttf_buffer: %p, data: %p, user_data: %p, font_start: %d, hhea: %d", font->ttf_buffer, font->stbfont.data, font->stbfont.userdata, font->stbfont.fontstart, font->stbfont.hhea);
        }
        else
        {
            LOG_DEBUG("Cannot load font without a file");
        }
    }
}

#define assets_get_font(assets, index) cast(Loaded_Font *) assets_get_(assets, index, Asset_Type_Font)
#define assets_get_image(assets, index) cast(Loaded_Image *) assets_get_(assets, index, Asset_Type_Image)
#define assets_get_data(assets, index) cast(Loaded_Data *) assets_get_(assets, index, Asset_Type_Data)
internal void *
assets_get_(Zhc_Assets *assets, Asset_ID index, Asset_Type type)
{
    void *result = 0;

    Asset *asset = assets->assets + index;
    switch(type)
    {
        case Asset_Type_Font:
        {
            assets_load_font(assets, index);
            if(asset->header) { result = &asset->header->font; }
        } break;
        case Asset_Type_Image:
        {
            // TODO(dgl): try to load asset
            if(asset->header) { result = &asset->header->image; }
        } break;
        case Asset_Type_Data:
        {
            // TODO(dgl): try to load asset
            if(asset->header) { result = &asset->header->data; }
        } break;
        default: { LOG_DEBUG("Unsupported asset type %d", type); }
    }

    if(!asset->header)
    {
        LOG_DEBUG("Failed to load asset %d dynamically.", index);
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

internal Asset_ID
assets_push_file(Zhc_Assets *assets, Zhc_File_Handle handle, usize size)
{
    Asset_ID result = assets_push(assets);
    assets->files = dgl_mem_arena_resize_array(assets->permanent_arena, Asset_File, assets->files, cast(usize)assets->file_count, cast(usize)(assets->file_count + 1));

    Asset_File *file = assets->files + assets->file_count++;
    file->asset = result;
    file->handle = handle;
    file->size = size;

    return(result);
}

// NOTE(dgl): file if is the same as the list element in group
// TODO(dgl): Find better way to identify files.
internal Zhc_Assets *
assets_begin_allocate(DGL_Mem_Arena *permanent_arena, usize memory_size)
{
    Zhc_Assets *result = dgl_mem_arena_push_struct(permanent_arena, Zhc_Assets);
    result->permanent_arena = permanent_arena;

    result->memory = dgl_mem_arena_push_array(result->permanent_arena, uint8, memory_size);

    result->memory_sentinel.next = &result->memory_sentinel;
    result->memory_sentinel.prev = &result->memory_sentinel;
    result->header_sentinel.next = &result->header_sentinel;
    result->header_sentinel.prev = &result->header_sentinel;

    insert_block(&result->memory_sentinel, result->memory, memory_size);

    result->asset_count = 0;

    // NOTE(dgl): we create an invalid file on index 0. This is the default
    // file for all assets not backed by a file.
    result->file_count = 1;

    // NOTE(dgl): must be last allocation in this function, because we resize on each pushed file.
    result->files = dgl_mem_arena_push_array(result->permanent_arena, Asset_File, cast(usize)result->file_count);
    return(result);
}

internal void
assets_end_allocate(Zhc_Assets *assets)
{

    assets->assets = dgl_mem_arena_push_array(assets->permanent_arena, Asset, cast(usize)assets->asset_count);

    // NOTE(dgl): put file ids into assets. If assets do not have a file id, they are child assets
    // of others (like the font bitmap).
    for(int index = 1; index < assets->file_count; ++index)
    {
        Asset_File *file = assets->files + index;
        Asset *asset = assets->assets + file->asset;

        asset->file_index = index;
    }
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
