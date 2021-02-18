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
    int32 min_x = rect.x;
    int32 min_y = rect.y;
    int32 max_x = rect.x + rect.w;
    int32 max_y = rect.y + rect.h;
    //LOG_DEBUG("Drawing rectangle: min X: %d, min Y: %d, max X: %d, max Y: %d", min_x, min_y, max_x, max_y);


    min_x = dgl_clamp(min_x, 0, min_x);
    min_y = dgl_clamp(min_y, 0, min_y);
    max_x = dgl_clamp(max_x, max_x, buffer->width);
    max_y = dgl_clamp(max_y, max_y, buffer->height);

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
ren_draw_bitmap(Zhc_Offscreen_Buffer *buffer, Zhc_Image *image, V4 rect, V2 pos, V4 color)
{
    // TODO(dgl): Something is not right. An image 100x100 is drawm only about 1/3.

    // TODO(dgl): refactor image and offset position
    // this is currently not overflow safe.

    int32 src_min_x = dgl_clamp(rect.x, 0, image->width);
    int32 src_min_y = dgl_clamp(rect.y, 0, image->height);
    int32 src_max_x = dgl_clamp(rect.x + rect.w, src_min_x, image->width);
    int32 src_max_y = dgl_clamp(rect.y + rect.h, src_min_y, image->height);

    int32 dest_min_x = dgl_clamp(pos.x, 0, buffer->width);
    int32 dest_min_y = dgl_clamp(pos.y, 0, buffer->height);
    int32 dest_max_x = dgl_clamp(pos.x + rect.w, dest_min_x, buffer->width);
    int32 dest_max_y = dgl_clamp(pos.y + rect.h, dest_min_y, buffer->height);

    uint32 *source_row = image->pixels +
                         src_min_x +
                         src_min_y*image->width;

    uint8 *dest_row = ((uint8 *)buffer->memory +
                      dest_min_x*buffer->bytes_per_pixel +
                      dest_min_y*buffer->pitch);


    int32 dest_y = dest_min_y;
    int32 src_y = src_min_y;
    while(src_y < src_max_y)
    {
        uint32 *source = source_row;
        uint32 *dest = (uint32 *)dest_row;
        int32 src_x = src_min_x;
        int32 dest_x = dest_min_x;

        while(src_x < src_max_x)
        {
            if(dest_y < dest_max_y && dest_x < dest_max_x)
            {
                *dest = blend_pixel(*source, *dest, color);
            }

            source++;
            dest++;
            src_x++;
            dest_x++;

        }

        dest_row += buffer->pitch;
        source_row += image->width;

        src_y++;
        dest_y++;
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
