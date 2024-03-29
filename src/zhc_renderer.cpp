internal void
render_command_buffer_init(Render_Command_Buffer *buffer, uint8 *base, usize size)
{
    buffer->size = size;
    buffer->base = base;
    buffer->offset = 0;
    buffer->is_initialized = true;
}

#define render_command_by_type(cmd, type) cast(type *) render_command_by_type_internal(cmd)
internal inline void *
render_command_by_type_internal(Render_Command *cmd)
{
    uint8 *base = cast(uint8 *)cmd;
    void *result = base + sizeof(*cmd);
    return(result);
}

#define render_command_alloc(buffer, command_type, type) cast(type *) render_command_alloc_internal(buffer, command_type, sizeof(type));
internal void *
render_command_alloc_internal(Render_Command_Buffer *buffer, Render_Command_Type type, usize size)
{
    assert(buffer->is_initialized, "Render buffer is not initialized");
    assert(buffer->offset + sizeof(Render_Command) + size < buffer->size, "Render buffer overflow");
    uint8 *current_base = buffer->base + buffer->offset;
    Render_Command *cmd = cast(Render_Command *)current_base;
    cmd->size = size;
    cmd->type = type;

    buffer->offset += sizeof(*cmd) + size;
    void *result = current_base + sizeof(*cmd);

    assert(result == (buffer->base + buffer->offset - size), "Invalid render command pointer");
    return(result);
}

internal Render_Command *
get_next_command(Render_Command_Buffer *buffer, Render_Command *cmd)
{
    Render_Command *result = 0;

    assert(buffer->offset < buffer->size, "Offset cannot be bigger than the buffer size");

    uint8 *base = cast(uint8 *)cmd;
    uint8 *next = base + (sizeof(*cmd) + cmd->size);

    if(next < (buffer->base + buffer->offset))
    {
        result = cast(Render_Command *)next;
    }

    return(result);
}

internal void
renderer_build_glyphs(Zhc_Assets *assets, Loaded_Font *font, Asset_ID bitmap_id, int32 font_size)
{
    // Get font metrics
    int32 ascent, descent, linegap;
    stbtt_GetFontVMetrics(&font->stbfont, &ascent, &descent, &linegap);

    real32 scale = stbtt_ScaleForPixelHeight(&font->stbfont, cast(real32)font_size);
    // NOTE(dgl): linegap is defined by the font. However it was 0 in the fonts I
    // have tested.
    font->linegap = 1.4f; //cast(real32)linegap;
    font->height = cast(real32)(ascent - descent) * scale;
    font->size = font_size;

    // NOTE(dgl): loading the font_bitmap, if it exists. Otherwise this pointer is NULL!
    Loaded_Image *font_bitmap = assets_get_image(assets, bitmap_id);
    int32 bitmap_width = 128;
    int32 bitmap_height = 128;

retry:
    int32 pixel_count = bitmap_width * bitmap_height;
    if(font_bitmap)
    {
        LOG_DEBUG("Unloading bitmap buffer (%dx%d) for resizing", bitmap_width, bitmap_height);
        assets_unload(assets, bitmap_id);
    }

    LOG_DEBUG("Loading bitmap buffer %dx%d", bitmap_width, bitmap_height);
    assets_allocate_image(assets, bitmap_id, bitmap_width, bitmap_height);
    font_bitmap = assets_get_image(assets, bitmap_id);

    real32 s = stbtt_ScaleForMappingEmToPixels(&font->stbfont, 1) / stbtt_ScaleForPixelHeight(&font->stbfont, 1);

    /* load glyphs */
    int32 success = stbtt_BakeFontBitmap(font->ttf_buffer, 0, cast(real32)font->size * s,
                                         cast(uint8 *)font_bitmap->pixels, font_bitmap->width, font_bitmap->height,
                                         0, font->glyph_count, font->glyphs);

    if(success < 0)
    {
        LOG_DEBUG("Could not fit the characters into the bitmap (%dx%d). Retrying...", font_bitmap->width, font_bitmap->height);
        bitmap_width *= 2;
        bitmap_height *= 2;
        goto retry;
    }

    // map 8bit Bitmap to 32bit
    for(int32 index = pixel_count - 1;
        index >= 0;
        --index)
    {
        // NOTE(dgl): we only store the alpha channel.
        // the others are set on drawing.
        uint8 alpha = *(cast(uint8 *)font_bitmap->pixels + index);
        font_bitmap->pixels[index] = (cast(uint32)(alpha << 24) |
                                             (0xFF << 16) |
                                             (0xFF << 8) |
                                             (0xFF << 0));
    }

    // make tab and newline glyphs invisible
    font->glyphs[cast(int32)'\t'].x1 = font->glyphs[cast(int32)'\t'].x0;
    font->glyphs[cast(int32)'\n'].x1 = font->glyphs[cast(int32)'\n'].x0;
}

internal Render_Glyph
renderer_get_glyph_rect(Loaded_Font *font, uint32 codepoint)
{
    Render_Glyph result = {};
    if(codepoint >= font->glyph_count)
    {
        LOG("Glyph cannot be drawn. We currently support only %d glyphs", font->glyph_count);
        codepoint = 0;
    }

    stbtt_bakedchar raw_glyph = font->glyphs[codepoint];
    result.coordinates = v4(raw_glyph.x0, raw_glyph.y0, raw_glyph.x1 - raw_glyph.x0, raw_glyph.y1 - raw_glyph.y0);
    result.offset = v2(dgl_round_real32_to_int32(raw_glyph.xoff), dgl_round_real32_to_int32(raw_glyph.yoff));

    return(result);
}

internal Hash_Grid *
renderer_buid_hash_grid(DGL_Mem_Arena *arena, int32 cell_count_x, int32 cell_count_y)
{
    Hash_Grid *result = 0;

    result = dgl_mem_arena_push_struct(arena, Hash_Grid);
    result->cell_count_x = cell_count_x;
    result->cell_count_y = cell_count_y;
    usize cells = cast(usize)(cell_count_x * cell_count_y);
    result->cells = dgl_mem_arena_push_array(arena, uint32, cells);
    result->prev_cells = dgl_mem_arena_push_array(arena, uint32, cells);
    // NOTE(dgl): Worst case should be every other cells gets rendered. Then we need half the cells.
    // Adjacent cells get merged
    result->render_rect_count = dgl_safe_size_to_int32((cells / 2) + 1);
    result->render_rects = dgl_mem_arena_push_array(arena, V4, cast(usize)result->render_rect_count);

    return(result);
}

internal void
update_hash_grid(Zhc_Assets *assets, Hash_Grid *grid, Zhc_Offscreen_Buffer *buffer, V4 rect, Render_Command *cmd)
{
    uint32 cmd_hash = HASH_OFFSET_BASIS;
    uint8 *command_base = cast(uint8*)cmd + sizeof(*cmd);

    // NOTE(dgl): just as a safety measure.
    assert(command_base == cast(uint8 *)render_command_by_type_internal(cmd), "Invalid command base");
    hash(&cmd_hash, command_base, cmd->size);

    real32 scale_width = cast(real32)grid->cell_count_x / cast(real32)buffer->width;
    real32 scale_height = cast(real32)grid->cell_count_y / cast(real32)buffer->height;

    int32 rect_x1 = dgl_max(cast(int32)(cast(real32)rect.x * scale_width), 0);
    int32 rect_y1 = dgl_max(cast(int32)(cast(real32)rect.y * scale_height), 0);
    int32 rect_x2 = dgl_min(cast(int32)(cast(real32)(rect.x + rect.w) * scale_width), grid->cell_count_x - 1);
    int32 rect_y2 = dgl_min(cast(int32)(cast(real32)(rect.y + rect.h) * scale_height), grid->cell_count_y - 1);

    // NOTE(dgl): must be inclusive because the cell number is truncated.
    for(int y = rect_y1; y <= rect_y2; y++)
    {
        for(int x = rect_x1; x <= rect_x2; x++)
        {
            int32 index = (x + (y * grid->cell_count_x));
            assert(index < grid->cell_count_x * grid->cell_count_y && index >= 0, "Grid cell overflow. Index is out of bounds");

            uint32 *cell = &grid->cells[index];
            hash(cell, &cmd_hash, sizeof(cmd_hash));

#if 0
            LOG_DEBUG("Hash after hashing:  %.10u, Prev_Hash %.10u", grid->cells[5], grid->prev_cells[5]);
            switch(cmd->type)
            {
                case Render_Command_Type_Rect_Filled:
                {
                     Render_Command_Rect *rect_cmd = render_command_by_type(cmd, Render_Command_Rect);
                     printf("ID %d, Cmd %u, Cell %u - Rect - rect x: %d, y: %d, w: %d, h: %d, color: r: %f, g: %f, b: %f, a: %f\n", index, cmd_hash, *cell, rect_cmd->rect.x, rect_cmd->rect.y, rect_cmd->rect.w, rect_cmd->rect.h, rect_cmd->color.r, rect_cmd->color.g, rect_cmd->color.b, rect_cmd->color.a);
                } break;
                case Render_Command_Type_Image:
                {
                    Render_Command_Image *img_cmd = render_command_by_type(cmd, Render_Command_Image);
                    printf("ID %d, Cmd %u, Cell %u - Image - pos: x: %d, y: %d, rect x: %d, y: %d, w: %d, h: %d, color: r: %f, g: %f, b: %f, a: %f\n", index, cmd_hash, *cell, img_cmd->pos.x, img_cmd->pos.y, img_cmd->rect.x, img_cmd->rect.y, img_cmd->rect.w, img_cmd->rect.h, img_cmd->color.r, img_cmd->color.g, img_cmd->color.b, img_cmd->color.a);
                } break;
                case Render_Command_Type_Font:
                {
                    Render_Command_Font *font_cmd = render_command_by_type(cmd, Render_Command_Font);

                    Loaded_Font *font = assets_get_font(assets, font_cmd->font);
                    if(font->size != font_cmd->size)
                    {
                        renderer_build_glyphs(assets, font, font_cmd->bitmap, font_cmd->size);
                    }

                    Render_Glyph glyph = renderer_get_glyph_rect(font, font_cmd->codepoint);
                    V2 real_pos = v2_add(glyph.offset, font_cmd->pos);
                    printf("ID %d, Cmd %u, Cell %u - Font - pos: x: %d, y: %d, real_pos: x: %d, y: %d, color: r: %f, g: %f, b: %f, a: %f, codepoint: %u, font %d, bitmap %d, size: %d\n", index, cmd_hash, *cell, font_cmd->pos.x, font_cmd->pos.y, real_pos.x, real_pos.y, font_cmd->color.r, font_cmd->color.g, font_cmd->color.b, font_cmd->color.a, font_cmd->codepoint, font_cmd->font, font_cmd->bitmap, font_cmd->size);
                } break;
                default:
                {
                    //LOG_DEBUG("Nothing rendered");
                }
            }
#endif
        }
    }
}

inline uint32
blend_pixel(uint32 src, uint32 dest, V4 color_)
{
    uint32 result = 0;

    real32 A = ((real32)((src >> 24) & 0xFF) / 255.0f) * color_.a;

    real32 SR = (real32)((src >> 16) & 0xFF);
    real32 SG = (real32)((src >> 8) & 0xFF);
    real32 SB = (real32)((src >> 0) & 0xFF);

    real32 DR = (real32)((dest >> 16) & 0xFF);
    real32 DG = (real32)((dest >> 8) & 0xFF);
    real32 DB = (real32)((dest >> 0) & 0xFF);

    // NOTE(dgl): Do linear blend for each color
    // Linear Blend = (1 - t)*Source + t*Dest
    // B = A + (B - A) -> C = A + t(B - A) => Day 38 1:33:00h
    real32 R = (1.0f-A)*DR + (A*SR*color_.r);
    real32 G = (1.0f-A)*DG + (A*SG*color_.g);
    real32 B = (1.0f-A)*DB + (A*SB*color_.b);

    // TODO(dgl): checkout premultiplied alpha
    result = (((uint32)(R + 0.5f) << 16) |
             ((uint32)(G + 0.5f) << 8)  |
             ((uint32)(B + 0.5f) << 0));

    return(result);
}

inline V4
intersect_rect(V4 a, V4 b)
{
    V4 result = {};

    int32 x0 = dgl_max(a.x, b.x);
    int32 y0 = dgl_max(a.y, b.y);
    int32 x1 = dgl_min((a.x + a.w), (b.x + b.w));
    int32 y1 = dgl_min((a.y + a.h), (b.y + b.h));

    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;

    result = v4(x0, y0, x1 - x0, y1 - y0);
    return(result);
}

internal void
draw_rectangle(Render_Context *ctx, Zhc_Offscreen_Buffer *buffer, V4 rect, V4 color)
{
    V4 clipped = intersect_rect(ctx->clipping_rect, rect);
    int32 min_x = clipped.x;
    int32 min_y = clipped.y;
    int32 max_x = clipped.x + clipped.w;
    int32 max_y = clipped.y + clipped.h;

#if 0
    LOG_DEBUG("Buffer W: %d H: %d", buffer->width, buffer->height);
    LOG_DEBUG("Drawing Rect - min_x %d, min_y %d, max_x %d, max_y %d", min_x, min_y, max_x, max_y);
#endif
    assert(min_x >= 0 && min_y >= 0 && max_x <= buffer->width && max_y <= buffer->height, "Render buffer overflow. Rect does not fit on screen");

    uint8 *row = (cast(uint8 *)buffer->memory +
                  min_y*buffer->pitch +
                  min_x*buffer->bytes_per_pixel);

    for(int y = min_y;
        y < max_y;
        ++y)
    {
        uint32 *pixel = cast(uint32 *)row;
        for(int x = min_x;
            x < max_x;
            ++x)
        {
            *pixel = blend_pixel(0xFFFFFFFF, *pixel, color);
            ++pixel;
        }

        row += buffer->pitch;
    }
}

internal void
draw_bitmap(Render_Context *ctx, Zhc_Offscreen_Buffer *buffer, Loaded_Image *bitmap, V4 rect, V2 pos, V4 color)
{
    assert(bitmap, "Image is not initialized");

    assert(rect.x >= 0, "Rect x cannot be smaller than 0");
    assert(rect.y >= 0, "Rect x cannot be smaller than 0");
    assert(rect.w <= bitmap->width, "Rect w cannot be greater than the image width");
    assert(rect.h <= bitmap->height, "Rect h cannot be greater than the image height");

    V2 source_pos = v2(rect.x, rect.y);
    V4 target_rect = v4(pos.x, pos.y, rect.w, rect.h);

    V4 clipped = intersect_rect(ctx->clipping_rect, target_rect);

    // NOTE(dgl): We increase the min_x and min_y positions by the underflow to
    // have them always larger than 0
    int32 clip_diff_x = clipped.x - pos.x;
    int32 clip_diff_y = clipped.y - pos.y;

    int32 min_x = clipped.x;
    int32 min_y = clipped.y;
    int32 max_x = dgl_min(clipped.x + clipped.w, buffer->width);
    int32 max_y = dgl_min(clipped.y + clipped.h, buffer->height);

#if 0
    LOG_DEBUG("Drawing Bitmap - min_x %d, min_y %d, max_x %d, max_y %d", min_x, min_y, max_x, max_y);
#endif

    // NOTE(dgl): we apply the underflow here to start copying the pixels from the correct
    // position. Otherwise we would always start from the top. But if we scroll e.g. upwrads
    // the top must be hidden and only the bottom should be drawn.
    uint32 *source_row = bitmap->pixels +
                         (source_pos.x + clip_diff_x) +
                         (source_pos.y + clip_diff_y)*bitmap->width;

    uint8 *dest_row = ((uint8 *)buffer->memory +
                      min_x*buffer->bytes_per_pixel +
                      min_y*buffer->pitch);

    for(int32 y = min_y;
        y < max_y;
        ++y)
    {
        uint32 *source = source_row;
        uint32 *dest = (uint32 *)dest_row;

        for(int32 x = min_x;
           x < max_x;
           ++x)
        {
            *dest = blend_pixel(*source, *dest, color);
            dest++;
            source++;
        }

        dest_row += buffer->pitch;
        source_row += bitmap->width;
    }
}

internal void
render(Render_Context *ctx, Render_Command_Buffer *commands, Zhc_Assets *assets, Zhc_Offscreen_Buffer *screen_buffer)
{
#if CACHED_RENDERING == true
    Hash_Grid *grid = ctx->grid;
    uint32 *tmp = grid->cells;
    grid->cells = grid->prev_cells;
    grid->prev_cells = tmp;

    // NOTE(dgl): resetting hash grid
    for (int index = 0; index < grid->cell_count_x * grid->cell_count_y; ++index)
    {
      grid->cells[index] = HASH_OFFSET_BASIS;
    }

    // NOTE(dgl): create hashes in grid to setup the rect to render.
    Render_Command *cmd = cast(Render_Command *)commands->base;
    while(cmd)
    {
        switch(cmd->type)
        {
            case Render_Command_Type_Rect_Filled:
            {
                 Render_Command_Rect *rect_cmd = render_command_by_type(cmd, Render_Command_Rect);
                 update_hash_grid(assets, grid, screen_buffer, rect_cmd->rect, cmd);
            } break;
            case Render_Command_Type_Image:
            {
                Render_Command_Image *img_cmd = render_command_by_type(cmd, Render_Command_Image);
                V4 render_rect = v4(img_cmd->pos.x, img_cmd->pos.y, img_cmd->rect.w, img_cmd->rect.h);
                update_hash_grid(assets, grid, screen_buffer, render_rect, cmd);
            } break;
            case Render_Command_Type_Font:
            {
                Render_Command_Font *font_cmd = render_command_by_type(cmd, Render_Command_Font);

                Loaded_Font *font = assets_get_font(assets, font_cmd->font);
                if(font->size != font_cmd->size)
                {
                    renderer_build_glyphs(assets, font, font_cmd->bitmap, font_cmd->size);
                }

                Render_Glyph glyph = renderer_get_glyph_rect(font, font_cmd->codepoint);
                V2 real_pos = v2_add(glyph.offset, font_cmd->pos);
                V4 render_rect = v4(real_pos.x, real_pos.y, glyph.coordinates.w, glyph.coordinates.h);
                update_hash_grid(assets, grid, screen_buffer, render_rect, cmd);
            } break;
            default:
            {
                LOG_DEBUG("Render command type %d ignored", cmd->type);
            }
        }
        //LOG_DEBUG("Hash Updated Cell 5 Hash: %u, prev: %u", grid->cells[5], grid->prev_cells[5]);
        cmd = get_next_command(commands, cmd);
    }

#if CACHED_RENDERING_DEBUG == true
    local_persist int color_index;
    color_index = ++color_index % 5;
#endif

    // NOTE(dgl): check if cells need update and can be merged into render rects
    int32 render_rect_count = 0;
    ctx->clipping_rect = v4(0,0,screen_buffer->width, screen_buffer->height);
    for(int32 y = 0; y < grid->cell_count_y; ++y)
    {
        for(int32 x = 0; x < grid->cell_count_x; ++x)
        {
            int32 index = x + (y * grid->cell_count_x);
            //LOG_DEBUG("Cell Index: %d - Prev hash %u <=> hash %u", index, grid->prev_cells[index], grid->cells[index]);
            // NOTE(dgl): if a cell is rendered the prev_cell gehts the cell hash to avoid cells being rendered twice.
            if(grid->cells[index] !=  grid->prev_cells[index])
            {
                //LOG_DEBUG("NOT EQUAL, WILL RENDER");
                V4 cell = v4(x, y, 1, 1);
                int32 prev_rect = dgl_max(render_rect_count - 1, 0);
                V4 render_rect = grid->render_rects[prev_rect];
                if(render_rect_count > 0 &&
                   cell.x + cell.w >= render_rect.x && cell.x <= render_rect.x + render_rect.w &&
                   cell.y + cell.h >= render_rect.y && cell.y <= render_rect.y + render_rect.h)
                {
                      int32 x1 = dgl_min(cell.x, render_rect.x);
                      int32 y1 = dgl_min(cell.y, render_rect.y);
                      int32 x2 = dgl_max(cell.x + cell.w, render_rect.x + render_rect.w);
                      int32 y2 = dgl_max(cell.y + cell.h, render_rect.y + render_rect.h);
                      V4 merged = v4(x1, y1, x2 - x1, y2 - y1);
                      grid->render_rects[prev_rect] = merged;
                }
                else // NOTE(dgl): not mergeable
                {
                    assert(render_rect_count < grid->render_rect_count, "Render rect overflow");
                    grid->render_rects[render_rect_count++] = cell;
                }
            }
        }
    }


    // NOTE(dgl): render to screen buffer
    for(int32 index = 0; index < render_rect_count; ++index)
    {
        V4 rect = grid->render_rects[index];

        real32 scale_width = cast(real32)screen_buffer->width / cast(real32)grid->cell_count_x;
        real32 scale_height = cast(real32)screen_buffer->height / cast(real32)grid->cell_count_y;

        int32 x = cast(int32)(cast(real32)rect.x * scale_width);
        int32 y = cast(int32)(cast(real32)rect.y * scale_height);
        int32 w = cast(int32)(cast(real32)rect.w * scale_width);
        int32 h = cast(int32)(cast(real32)rect.h * scale_height);

        ctx->clipping_rect = v4(x, y, w, h);
#else // not cached rendering
    for(int32 index = 0; index < 1; ++index)
    {
        ctx->clipping_rect = v4(0, 0, screen_buffer->width, screen_buffer->height);
#endif // cached rendering
        Render_Command *cmd = cast(Render_Command *)commands->base;
        while(cmd)
        {
            switch(cmd->type)
            {
                case Render_Command_Type_Rect_Filled:
                {
                     Render_Command_Rect *rect_cmd = render_command_by_type(cmd, Render_Command_Rect);
//                              LOG_DEBUG("Rect - rect x: %d, y: %d, w: %d, h: %d, color: r: %f, g: %f, b: %f, a: %f", rect_cmd->rect.x, rect_cmd->rect.y, rect_cmd->rect.w, rect_cmd->rect.h, rect_cmd->color.r, rect_cmd->color.g, rect_cmd->color.b, rect_cmd->color.a);
                     draw_rectangle(ctx, screen_buffer, rect_cmd->rect, rect_cmd->color);
                } break;
                case Render_Command_Type_Image:
                {
                    Render_Command_Image *img_cmd = render_command_by_type(cmd, Render_Command_Image);
                    Loaded_Image *image = assets_get_image(assets, img_cmd->image);
//                             LOG_DEBUG("Image - pos: x: %d, y: %d, rect x: %d, y: %d, w: %d, h: %d, color: r: %f, g: %f, b: %f, a: %f", img_cmd->pos.x, img_cmd->pos.y, img_cmd->rect.x, img_cmd->rect.y, img_cmd->rect.w, img_cmd->rect.h, img_cmd->color.r, img_cmd->color.g, img_cmd->color.b, img_cmd->color.a);
                    draw_bitmap(ctx, screen_buffer, image, img_cmd->rect, img_cmd->pos, img_cmd->color);
                } break;
                case Render_Command_Type_Font:
                {
                    Render_Command_Font *font_cmd = render_command_by_type(cmd, Render_Command_Font);

                    Loaded_Font *font = assets_get_font(assets, font_cmd->font);
                    if(font->size != font_cmd->size)
                    {
                        renderer_build_glyphs(assets, font, font_cmd->bitmap, font_cmd->size);
                    }

                    Render_Glyph glyph = renderer_get_glyph_rect(font, font_cmd->codepoint);

                    V2 real_pos = v2_add(glyph.offset, font_cmd->pos);
                    Loaded_Image *bitmap = assets_get_image(assets, font_cmd->bitmap);
//                             LOG_DEBUG("Font - pos: x: %d, y: %d, color: r: %f, g: %f, b: %f, a: %f, codepoint: %u, font %d, bitmap %d, size: %d", font_cmd->pos.x, font_cmd->pos.y, font_cmd->color.r, font_cmd->color.g, font_cmd->color.b, font_cmd->color.a, font_cmd->codepoint, font_cmd->font, font_cmd->bitmap, font_cmd->size);
                    draw_bitmap(ctx, screen_buffer, bitmap, glyph.coordinates, real_pos, font_cmd->color);
                } break;
                default:
                {
                    //LOG_DEBUG("Nothing rendered");
                }
            }

            cmd = get_next_command(commands, cmd);
        }

#if CACHED_RENDERING_DEBUG == true
        V4 colors[] = {V4{.r=1.0f, .g=0.0f, .b=0.0f, .a=0.2f},
                       V4{.r=0.0f, .g=1.0f, .b=0.0f, .a=0.2f},
                       V4{.r=0.0f, .g=0.0f, .b=1.0f, .a=0.2f},
                       V4{.r=1.0f, .g=1.0f, .b=0.0f, .a=0.2f},
                       V4{.r=0.0f, .g=1.0f, .b=1.0f, .a=0.2f}};
        draw_rectangle(ctx, screen_buffer, ctx->clipping_rect, colors[color_index]);
#endif
    }
}

#if 0
// NOTE(dgl): ignores newline characters.
internal void
draw_text(Zhc_Offscreen_Buffer *buffer, Font *font, char *text, int32 byte_count, V2 pos, V4 color_)
{
    char *curr_char = text;
    int32 x = pos.x;
    uint32 codepoint = 0;
    while(*curr_char && (byte_count-- > 0))
    {
        // TODO(dgl): ignore newline characters.
        if(*curr_char == '\n')
        {
            continue;
        }

        curr_char += utf8_to_codepoint(curr_char, &codepoint);
        stbtt_bakedchar *glyph = font->glyphs + codepoint;

        draw_bitmap(buffer,
                   font->bitmap,
                   v4(glyph->x0, glyph->y0, glyph->x1 - glyph->x0, glyph->y1 - glyph->y0),
                   v2(x + dgl_round_real32_to_int32(glyph->xoff), pos.y + dgl_round_real32_to_int32(glyph->yoff)), color_);

        x += dgl_round_real32_to_int32(glyph->xadvance);
    }
}
#endif
