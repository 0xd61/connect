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

    if(next < buffer->base + buffer->offset)
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

internal void
draw_rectangle(Zhc_Offscreen_Buffer *buffer, V4 rect, V4 color)
{
    // TODO(dgl): check clamping.
    int32 min_x = dgl_max(0, rect.x);
    int32 min_y = dgl_max(0, rect.y);
    int32 max_x = dgl_min(rect.x + rect.w, buffer->width);
    int32 max_y = dgl_min(rect.y + rect.h, buffer->height);

#if 0
    LOG_DEBUG("Buffer W: %d H: %d", buffer->width, buffer->height);
    LOG_DEBUG("Drawing rectangle: min X: %d, min Y: %d, max X: %d, max Y: %d", min_x, min_y, max_x, max_y);
#endif

    // TODO(dgl): do we have to check if min_x < max_y etc.?
    min_x = dgl_clamp(min_x, 0, max_x);
    min_y = dgl_clamp(min_y, 0, max_y);
    max_x = dgl_clamp(max_x, min_x, buffer->width);
    max_y = dgl_clamp(max_y, min_y, buffer->height);

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
draw_bitmap(Zhc_Offscreen_Buffer *buffer, Loaded_Image *bitmap, V4 rect, V2 pos, V4 color)
{
    assert(bitmap, "Image is not initialized");

    assert(rect.x >= 0, "Rect x cannot be smaller than 0");
    assert(rect.y >= 0, "Rect x cannot be smaller than 0");
    assert(rect.w <= bitmap->width, "Rect w cannot be greater than the image width");
    assert(rect.h <= bitmap->height, "Rect h cannot be greater than the image height");

    // NOTE(dgl): We increase the min_x and min_y positions by the underflow to
    // have them always larger than 0
    int32 underflow_x = dgl_min(pos.x, 0);
    int32 underflow_y = dgl_min(pos.y, 0);
    int32 min_x = pos.x - underflow_x;
    int32 min_y = pos.y - underflow_y;
    int32 max_x = dgl_min(pos.x + rect.w, buffer->width);
    int32 max_y = dgl_min(pos.y + rect.h, buffer->height);


    // NOTE(dgl): we apply the underflow here to start copying the pixels from the correct
    // position. Otherwise we would always start from the top. But if we scroll e.g. upwrads
    // the top must be hidden and only the bottom should be drawn.
    uint32 *source_row = bitmap->pixels +
                         (rect.x - underflow_x) +
                         (rect.y - underflow_y)*bitmap->width;

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

internal V2
v2_add(V2 a, V2 b)
{
    V2 result = {};
    result.x = a.x + b.x;
    result.y = a.y + b.y;

    return(result);
}

internal void
render(Render_Command_Buffer *commands, Zhc_Assets *assets, Zhc_Offscreen_Buffer *screen_buffer)
{
    Render_Command *cmd = cast(Render_Command *)commands->base;

    while(cmd)
    {
        switch(cmd->type)
        {
            case Render_Command_Type_Rect_Filled:
            {
                 Render_Command_Rect *rect_cmd = render_command_by_type(cmd, Render_Command_Rect);
                 draw_rectangle(screen_buffer, rect_cmd->rect, rect_cmd->color);
            } break;
            case Render_Command_Type_Image:
            {
                Render_Command_Image *img_cmd = render_command_by_type(cmd, Render_Command_Image);
                Loaded_Image *image = assets_get_image(assets, img_cmd->image);
                draw_bitmap(screen_buffer, image, img_cmd->rect, img_cmd->pos, img_cmd->color);
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
                draw_bitmap(screen_buffer, bitmap, glyph.coordinates, real_pos, font_cmd->color);
            }
            default:
            {
                //LOG_DEBUG("Nothing rendered");
            }
        }

        cmd = get_next_command(commands, cmd);
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
