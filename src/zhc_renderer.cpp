#include "zhc_platform.h"

#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#define STBTT_STATIC
#define STBTT_assert(x) assert(x, "stb assert")

// NOTE(dgl): Disable compiler warnings for stb includes
#if defined(__clang__)
#pragma clang diagnostic push
#if __clang_major__ > 7
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#endif
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wfloat-conversion"

#include "lib/stb_truetype.h"

#pragma clang diagnostic pop
#endif

// TODO(dgl): not tested
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning (disable: 4127)             // condition expression is constant
#pragma warning (disable: 4996)             // 'This function or variable may be unsafe': strcpy, strdup, sprintf, vsnprintf, sscanf, fopen
#if defined(_MSC_VER) && _MSC_VER >= 1922   // MSVC 2019 16.2 or later
#pragma warning (disable: 5054)             // operator '|': deprecated between enumerations of different types
#endif

#include "stb_truetype.h"

#pragma warning(pop)
#endif

typedef struct Image
{
    uint32 *pixels;
    int32 width;
    int32 height;
} Image;

struct Font
{
    uint8 *ttf_buffer;
    Image *bitmap;
    real32 size;
    real32 linegap;
    real32 height;
    stbtt_fontinfo stbfont;
    // NOTE(dgl): we only encode ASCII + Latin-1 (first 256 code points)
    // if we need more, use glyphsets with each 256 characters to reduce
    // the amount of memory needed.
    stbtt_bakedchar glyphs[256];
};

struct Render
{
    Zhc_Platform_Api api;
    Zhc_Offscreen_Buffer *draw_buffer;

    DGL_Mem_Arena arena;
    V4 clip;
    Font *font;

    bool32 is_initialized;
};

internal void
set_clip(Render *r, V4 rect)
{
    r->clip.left = rect.x;
    r->clip.top = rect.y;
    r->clip.right = rect.x + rect.w;
    r->clip.bottom = rect.y + rect.h;
}

internal Render *
get_renderer(Zhc_Memory *memory)
{
    assert(sizeof(Render) < memory->render_storage_size, "Not enough memory allocated");
    Render *result = cast(Render *)memory->render_storage;
    return(result);
}

internal Font *
initialize_font(DGL_Mem_Arena *arena, uint8 *ttf_buffer, real32 font_size)
{
    Font *result = dgl_mem_arena_push_struct(arena, Font);
    result->ttf_buffer = ttf_buffer;

    // init stbfont
    assert(stbtt_InitFont(&result->stbfont, result->ttf_buffer, stbtt_GetFontOffsetForIndex(result->ttf_buffer,0)), "Failed to load font");

    // get height and scale
    int32 ascent, descent, linegap;
    stbtt_GetFontVMetrics(&result->stbfont, &ascent, &descent, &linegap);
    result->size = font_size;
    real32 scale = stbtt_ScaleForPixelHeight(&result->stbfont, result->size);
    // NOTE(dgl): linegap is defined by the font. However it was 0 in the fonts I
    // have tested.
    result->linegap = 1.2f; //cast(real32)linegap;
    result->height = cast(real32)(ascent - descent) * scale;

    // build bitmap
    Image *bitmap = dgl_mem_arena_push_struct(arena, Image);
    bitmap->width = 128;
    bitmap->height = 128;
    int32 pixel_count = bitmap->width*bitmap->height;
    bitmap->pixels = dgl_mem_arena_push_array(arena, uint32, cast(usize)pixel_count);
    real32 s = stbtt_ScaleForMappingEmToPixels(&result->stbfont, 1) / stbtt_ScaleForPixelHeight(&result->stbfont, 1);

retry:
    /* load glyphs */
    int32 success = stbtt_BakeFontBitmap(result->ttf_buffer, 0, result->size*s,
                                         cast(uint8 *)bitmap->pixels, bitmap->width, bitmap->height,
                                         0, array_count(result->glyphs), result->glyphs);

    if(success < 0)
    {
        LOG_DEBUG("Could not fit the characters into the bitmap (%dx%d). Retrying...", bitmap->width, bitmap->height);
        bitmap->width *= 2;
        bitmap->height *= 2;
        bitmap->pixels = dgl_mem_arena_resize_array(arena, uint32, bitmap->pixels, cast(usize)pixel_count, cast(usize)(bitmap->width*bitmap->height));
        pixel_count = bitmap->width*bitmap->height;
        goto retry;
    }

    // map 8bit Bitmap to 32bit
    for(int32 index = pixel_count - 1;
        index >= 0;
        --index)
    {
        // NOTE(dgl): we only store the alpha channel.
        // the others are set on drawing.
        uint8 alpha = *(cast(uint8 *)bitmap->pixels + index);
        bitmap->pixels[index] = (cast(uint32)(alpha << 24) |
                                             (0xFF << 16) |
                                             (0xFF << 8) |
                                             (0xFF << 0));
    }
    result->bitmap = bitmap;

    // make tab and newline glyphs invisible
    result->glyphs[cast(int32)'\t'].x1 = result->glyphs[cast(int32)'\t'].x0;
    result->glyphs[cast(int32)'\n'].x1 = result->glyphs[cast(int32)'\n'].x0;

    return(result);
}

void
zhc_render_init(Zhc_Memory *memory, Zhc_Offscreen_Buffer *buffer)
{
    Render *r = get_renderer(memory);
    r->api = memory->api;
    r->draw_buffer = buffer;
    set_clip(r, rect(0,0,buffer->width, buffer->height));
    dgl_mem_arena_init(&r->arena, (uint8 *)memory->render_storage + sizeof(*r), (DGL_Mem_Index)memory->render_storage_size - sizeof(*r));

    // TODO(dgl): make this call relative.
    char *path = "/home/danielg/Code/C/connect/data/Inter-Regular.ttf";
    usize file_size = r->api.file_size(path);
    uint8 *ttf_buffer = dgl_mem_arena_push_array(&r->arena, uint8, file_size);
    bool32 success = r->api.read_entire_file(path, ttf_buffer, file_size);
    assert(success, "Could not initialize default font");
    r->font = initialize_font(&r->arena, ttf_buffer, 21.0f);

    r->is_initialized = true;
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
    result = ((dgl_round_real32_to_uint32(R) << 16) |
             (dgl_round_real32_to_uint32(G) << 8)  |
             (dgl_round_real32_to_uint32(B) << 0));

    return(result);
}

internal void
draw_rectangle(Zhc_Offscreen_Buffer *buffer, V4 rect, V4 color)
{
    int32 min_x = rect.x;
    int32 min_y = rect.y;
    int32 max_x = rect.x + rect.w;
    int32 max_y = rect.y + rect.h;
    LOG_DEBUG("Drawing rectangle: min X: %d, min Y: %d, max X: %d, max Y: %d", min_x, min_y, max_x, max_y);


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
draw_image(Zhc_Offscreen_Buffer *buffer, Image *image, V4 rect, V2 pos, V4 color)
{
    // TODO(dgl): Something is not right. An image 100x100 is drawm only about 1/3.

    // TODO(dgl): refactor image and offset position
    // this is currently not overflow safe.

    int32 src_min_x = dgl_clamp(rect.x, 0, image->width);
    int32 src_min_y = dgl_clamp(rect.y, 0, image->height);
    int32 src_max_x = dgl_clamp(rect.x + rect.w, src_min_x, image->width);
    int32 src_max_y = dgl_clamp(rect.y + rect.h, src_min_y, image->height);

    LOG_DEBUG("src_min_x %d, src_min_y %d, src_max_x %d, src_max_y %d", src_min_x, src_min_y, src_max_x , src_max_y);

    int32 dest_min_x = dgl_clamp(pos.x, 0, buffer->width);
    int32 dest_min_y = dgl_clamp(pos.y, 0, buffer->height);
    int32 dest_max_x = dgl_clamp(pos.x + rect.w, dest_min_x, buffer->width);
    int32 dest_max_y = dgl_clamp(pos.y + rect.h, dest_min_y, buffer->height);

    LOG_DEBUG("dest_min_x %d, dest_min_y %d, dest_max_x %d, dest_max_y %d", dest_min_x, dest_min_y, dest_max_x , dest_max_y);

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

internal int32
utf8_to_codepoint(char *c, uint32 *dest)
{
    int32 byte_count;
    uint32 codepoint;

    // NOTE(dgl): The first most significant 4 bits determine if the character
    // is encoded with 1, 2, 3 or 4 bytes.
    switch (*c & 0xf0)
    {
        // Byte 1     Byte 2     Byte 3     Byte 4
        // 1111 0xxx  10xx xxxx  10xx xxxx  10xx xxxx
        case 0xf0:
        {
            // we only want the 3 lsb of the first byte
            codepoint = cast(uint32)(*c & 0x07);
            byte_count = 4;
        } break;
        // Byte 1     Byte 2     Byte 3
        // 1110 xxxx  10xx xxxx  10xx xxxx
        case 0xe0:
        {
            // we only want the 4 lsb of the first byte
            codepoint = cast(uint32)(*c & 0x0f);
            byte_count = 3;
        } break;
        // Byte 1     Byte 2
        // 110x xxxx  10xx xxxx
        case 0xd0:
        case 0xc0:
        {
            // we only want the 5 lsb of the first byte
            codepoint = cast(uint32)(*c & 0x1f);
            byte_count = 2;
        } break;
        // Byte 1
        // 0xxx xxxx
        default:
        {
            codepoint = cast(uint32)(*c);
            byte_count = 1;
        } break;
    }

    int32 i = 0;
    while(++i < byte_count)
    {
        // NOTE(dgl): For the other bytes (2-4) we only want the 6 lsb.
        codepoint = (codepoint << 6) | (*(++c) & 0x3f);
    }

    *dest = codepoint;
    return(byte_count);
}

// NOTE(dgl): returns the x and y position after the
// latest drawn character.
internal V2
draw_text(Zhc_Offscreen_Buffer *buffer, Font *font, char *text, int32 byte_count, V2 pos, V4 color_)
{
    char *curr_char = text;
    int32 x = pos.x;
    int32 y = pos.y;
    uint32 codepoint = 0;
    while(*curr_char && (byte_count-- > 0))
    {
        // TODO(dgl): should we ignore the drawing of newline characters
        // and leave this to the caller function?
        if(*curr_char == '\n')
        {
            y += dgl_round_real32_to_int32(font->height + font->linegap);
            x = pos.x;
            ++curr_char;
            continue;
        }

        curr_char += utf8_to_codepoint(curr_char, &codepoint);
        stbtt_bakedchar *glyph = font->glyphs + codepoint;

        draw_image(buffer,
                   font->bitmap,
                   rect(glyph->x0, glyph->y0, glyph->x1 - glyph->x0, glyph->y1 - glyph->y0),
                   v2(x + dgl_round_real32_to_int32(glyph->xoff), y + dgl_round_real32_to_int32(glyph->yoff)), color_);

        x += dgl_round_real32_to_int32(glyph->xadvance);
    }

    V2 result = v2(x,y);
    return(result);
}

internal int32
get_font_width(Font *font, char *text, int32 byte_count)
{
    char *c = text;
    int32 result = 0;
    uint32 codepoint = 0;
    while(*c && (byte_count-- > 0))
    {
        c += utf8_to_codepoint(c, &codepoint);
        stbtt_bakedchar *glyph = font->glyphs + codepoint;
        result += dgl_round_real32_to_int32(glyph->xadvance);
    }

    return(result);
}

// NOTE(dgl): target is a string to support utf8 sequences
internal int32
next_word_byte_count(char *text)
{
    int32 result = 0;
    char *c = text;
    while(*c)
    {
        if((*c == ' ') || (*c == '\n') ||
           (*c == ';') || (*c == '-'))
        {
            break;
        }
        c++; result++;
    }

    return(result);
}


void
zhc_render_rect(Zhc_Memory *memory, V4 rect, V4 color)
{
    Render *r = get_renderer(memory);
    assert(r->is_initialized, "Render struct must be initialized");
    draw_rectangle(r->draw_buffer, rect, color);
}

void
zhc_render_text(Zhc_Memory *memory, V4 rect, V4 color, char *text)
{
    Render *r = get_renderer(memory);
    assert(r->is_initialized, "Render struct must be initialized");

    char *c = text;
    int32 y = rect.y + dgl_round_real32_to_int32(r->font->height);
    int32 x = rect.x;
    int32 max_x = rect.x + rect.w;

    // TODO(dgl): @@cleanup drawing with newline is kinda janky
    while(*c)
    {
        int32 word_byte_count = next_word_byte_count(c) + 1;
        int32 word_width = get_font_width(r->font, c, word_byte_count);

        if((x + word_width) > max_x)
        {
            y += dgl_round_real32_to_int32(r->font->height + r->font->linegap);
            x = rect.x;
        }

        V2 pos = draw_text(r->draw_buffer, r->font, c, word_byte_count, v2(x, y), color);

        // NOTE(dgl): check if we have had a newline
        if(pos.y > y)
        {
            y = pos.y;
            x = rect.x;
        }
        else
        {
            x += word_width;
        }
        c += word_byte_count;
    }
}
