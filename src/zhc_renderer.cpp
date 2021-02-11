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
    int32 width;
    int32 height;
    uint32 *pixels;
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
    result->linegap = cast(real32)linegap;
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

    if(!success)
    {
        LOG_DEBUG("Could not fit the characters into the bitmap. Retrying...");
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
    r->font = initialize_font(&r->arena, ttf_buffer, 12.0f);

    r->is_initialized = true;
}

void
zhc_render_rect(Zhc_Memory *memory)
{
    Render *r = get_renderer(memory);
    assert(r->is_initialized, "Render must be initialized");
    LOG_DEBUG("Render Rectangle");
}

void
zhc_render_text(Zhc_Memory *memory)
{
    Render *r = get_renderer(memory);
    assert(r->is_initialized, "Render must be initialized");
    LOG_DEBUG("Render Text");
}
