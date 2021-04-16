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
ren_draw_rectangle(Zhc_Offscreen_Buffer *buffer, V4 rect, V4 color)
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

internal V4
intersect_rect(V4 a, V4 b)
{
    V4 result = {};

    int32 x0 = dgl_max(a.x, b.x);
    int32 y0 = dgl_max(a.y, b.y);
    int32 x1 = dgl_min((a.x + a.w), (b.x + b.w));
    int32 y1 = dgl_min((a.y + a.h), (b.y + b.h));

    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;

    result = rect(x0, y0, x1 - x0, y1 - y0);
    return(result);
}

internal void
ren_draw_bitmap(Zhc_Offscreen_Buffer *buffer, Loaded_Image *image, V4 rect, V2 pos, V4 color)
{
    // NOTE(dgl): we do not clip the source. The caller must set the rect to the correct
    // dimension of the image.

    assert(rect.x >= 0, "Rect x cannot be smaller than 0");
    assert(rect.y >= 0, "Rect x cannot be smaller than 0");
    assert(rect.w <= image->width, "Rect w cannot be greater than the image width");
    assert(rect.h <= image->height, "Rect h cannot be greater than the image height");

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
    uint32 *source_row = image->pixels +
                         (rect.x - underflow_x) +
                         (rect.y - underflow_y)*image->width;

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
        source_row += image->width;
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
                   rect(glyph->x0, glyph->y0, glyph->x1 - glyph->x0, glyph->y1 - glyph->y0),
                   v2(x + dgl_round_real32_to_int32(glyph->xoff), pos.y + dgl_round_real32_to_int32(glyph->yoff)), color_);

        x += dgl_round_real32_to_int32(glyph->xadvance);
    }
}
#endif
