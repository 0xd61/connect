/*
    NOTE(dgl): TODOs
    - Socket for accepting clients
    - send data to clients
    - dynamic folder (where we search for the files) + folder dialog
    - config file
    - segfault if window too small
*/

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

#include "zhc_renderer.cpp"

#define Stack(Type) struct{usize count; usize offset; Type *memory;}

global Zhc_Platform_Api platform;

typedef uint32 Element_ID;

struct File
{
    Zhc_File_Info *info;
    uint8 *data;
};

struct Font
{
    uint8 *ttf_buffer;
    Zhc_Image *bitmap;
    real32 size;
    real32 linegap;
    real32 height;
    stbtt_fontinfo stbfont;
    // NOTE(dgl): we only encode ASCII + Latin-1 (first 256 code points)
    // if we need more, use glyphsets with each 256 characters to reduce
    // the amount of memory needed.
    stbtt_bakedchar glyphs[256];
};

struct Element_State
{
    Element_ID id;
    V2 content;
    int32 scroll_pos; /* only vertical scrolling */
};

struct Imui_Context
{
    V2 window;
    Element_ID active;
    Element_ID hot;
    // NOTE(dgl): the last hot element of the last frame
    // as a simple way to handle toverlapping elements.
    Element_ID top_most_hot;
    bool32 hot_updated;


    Font *system_font;

    V4 fg_color;
    V4 bg_color;

    // NOTE(dgl): to be able to reset the font
    // and have more safety, if we overflow the arena.
    DGL_Mem_Arena dyn_font_arena;
    Font *text_font;

    Zhc_Input *input;
    Zhc_Offscreen_Buffer *buffer;

    Stack(Element_ID) id_stack;
    Stack(Element_State) element_state_list;

    real32 desired_text_font_size;
    int32 desired_file_id;
};

struct Lib_State
{
    DGL_Mem_Arena permanent_arena;

    Imui_Context *ui_ctx;

    DGL_Mem_Arena io_arena; // NOTE(dgl): cleared on each update timeout
    real32 io_update_timeout;
    File active_file;
    Zhc_File_Group *files;


    bool32 is_initialized;
};

/* 32bit fnv-1a hash */
#define HASH_OFFSET_BASIS 0x811C9DC5
#define HASH_PRIME 0x01000193

internal void
hash(uint32 *hash, void *data, usize data_count)
{
    uint8 *octet = (uint8 *)data;
    while(data_count)
    {
        *hash = (*hash ^ *octet) * HASH_PRIME;
        data_count--;
        octet++;
    }
}

internal Element_ID
get_id(Imui_Context *ctx, void *data, usize data_count)
{
    usize index = ctx->id_stack.offset;
    Element_ID result;

    if(index > 0)
    {
        result = ctx->id_stack.memory[index - 1];
    }
    else
    {
        result = HASH_OFFSET_BASIS;
    }
    hash(&result, data, data_count);

    return(result);
}

internal bool32
input_down(Zhc_Input *input, Zhc_Mouse_Button button)
{
    bool32 result = (input->mouse_down & button) == button;
    return(result);
}

internal bool32
input_down(Zhc_Input *input, Zhc_Keyboard_Button button)
{
    bool32 result = (input->key_down & button) == button;
    return(result);
}

internal bool32
input_pressed(Zhc_Input *input, Zhc_Keyboard_Button button)
{
    bool32 result = (input->key_pressed & button) == button;
    return(result);
}

internal bool32
input_pressed(Zhc_Input *input, char character)
{
    bool32 result = input->text[0] == character;
    return(result);
}

internal Zhc_File_Info *
get_file_info(Zhc_File_Group *group, int32 index)
{
    Zhc_File_Info *result = 0;
    assert(group, "Group must be initialized");
    if(group->count > 0)
    {
        assert(index < group->count && index >= 0, "Index exceeds files in group");
        Zhc_File_Info *info = group->first_file_info;
        while(index-- > 0)
        {
            info = info->next;
        }

        result = info;
    }
    return(result);
}

internal File
read_active_file(DGL_Mem_Arena *arena, Zhc_File_Group *group, Zhc_File_Info *info)
{
    File result = {};

    if(group)
    {
        result.info = info;
        result.data = dgl_mem_arena_push_array(arena, uint8, result.info->size);

        // TODO(dgl): do proper string handling
        char filepath[1024] = {};

        sprintf(filepath, "%s/%s", group->dirpath, result.info->filename);
        LOG_DEBUG("Loading file %s", filepath);
        platform.read_entire_file(filepath, result.data, result.info->size);
    }

    return(result);
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

internal bool32
is_focused(Imui_Context *ctx, V4 rect)
{
    bool32 result = false;
    //V4 active_clip = ctx->clip_stack.memory[ctx->clip_stack.offset - 1];
    //V4 clipped = intersect_rect(rect, active_clip);

    Zhc_Input *input = ctx->input;
    int mouse_x = input->pos.x;
    int mouse_y = input->pos.y;

    result = (mouse_x > rect.x)           &&
             (mouse_x <= rect.x + rect.w) &&
             (mouse_y > rect.y)           &&
             (mouse_y <= rect.y + rect.h);

    return(result);
}

// NOTE(dgl): returns true, if id was active but is not anymore (e.g. if click happened)
internal bool32
update_control_state(Imui_Context *ctx, Element_ID id, V4 body)
{
#if 0
    LOG_DEBUG("Top most hot element: %u, Hot element %u, Active element: %u", ctx->top_most_hot, ctx->hot, ctx->active);
#endif
    bool32 result = false;
    bool32 mouse_left_down = input_down(ctx->input, Zhc_Mouse_Button_Left);

    if(ctx->active == id)
    {
        if(!mouse_left_down)
        {
            result = (ctx->top_most_hot == id);
            ctx->active = 0;
        }
    }
    else if(ctx->top_most_hot == id)
    {
        if(mouse_left_down)
        {
            ctx->active = id;

            // NOTE(dgl): we need to set the hot element here again
            // otherwise we have the hot id of element under this one.
            ctx->hot = ctx->top_most_hot;
        }
    }

    if(is_focused(ctx, body))
    {
        ctx->hot_updated = true;
        if(ctx->active == 0)
        {
            ctx->hot = id;
        }
    }

    return(result);
}

internal Element_State *
get_element_state(Imui_Context *ctx, Element_ID id)
{
    assert(ctx->element_state_list.memory, "element_state_list must be initialized");

    // NOTE(dgl): if performance is an issue, do an index lookup
    Element_State *result = 0;
    for(int32 index = 0; index < ctx->element_state_list.offset; ++index)
    {
        Element_State *c = ctx->element_state_list.memory + index;

        if(c->id == id)
        {
            result = c;
            break;
        }
    }

    if(!result)
    {
        assert(ctx->element_state_list.offset < ctx->element_state_list.count, "element_state_list overflow.");
        result = ctx->element_state_list.memory + ctx->element_state_list.offset++;
        result->id = id;
    }

    return(result);
}

internal bool32
begin_element(Imui_Context *ctx, Element_ID id, V4 rect)
{
    assert(ctx->id_stack.memory, "id_stack must be initialized");
    assert(ctx->id_stack.offset < ctx->id_stack.count, "id_stack overflow.");
    ctx->id_stack.memory[ctx->id_stack.offset++] = id;

    bool32 result = update_control_state(ctx, id, rect);
    return(result);
}

internal void
end_element(Imui_Context *ctx)
{
    assert(ctx->id_stack.offset > 0, "id_stack is empty. Check your begin and end elements.");
    ctx->id_stack.offset--;
}

internal Font *
initialize_font(DGL_Mem_Arena *arena, uint8 *ttf_buffer, real32 font_size)
{
    Font *result = dgl_mem_arena_push_struct(arena, Font);
    result->ttf_buffer = ttf_buffer;

    // init stbfont
    int32 init_success = stbtt_InitFont(&result->stbfont, result->ttf_buffer, stbtt_GetFontOffsetForIndex(result->ttf_buffer,0));
    assert(init_success, "Failed to load font");
    LOG_DEBUG("Font - ttf_buffer: %p, data: %p, user_data: %p, font_start: %d, hhea: %d", result->ttf_buffer, result->stbfont.data, result->stbfont.userdata, result->stbfont.fontstart, result->stbfont.hhea);

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
    Zhc_Image *bitmap = dgl_mem_arena_push_struct(arena, Zhc_Image);
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

// NOTE(dgl): width and height of the element (to calculate overflow)
internal V2
ui_textarea(Imui_Context *ctx, Font *font, V4 body, V4 color, char* text, int32 text_count)
{
    assert(font->bitmap, "Initialize font before rendering text");
    char *c = text;
    uint32 codepoint = 0;
    int32 x = body.x;
    int32 y = body.y + dgl_round_real32_to_int32(font->height);
    int32 current_w = 0;
    while((text_count > 0) && *c)
    {
        // NOTE(dgl): we add 1 to have the divider also rendered at the end of the word.
        // After drawing we check if the divider was a newline. If it was, we increase
        // y by one line. THe divier characters are all in ACII, hence only 1 byte long.
        int32 word_byte_count = next_word_byte_count(c) + 1;
        int32 word_width = get_font_width(font, c, word_byte_count);

        if(current_w + word_width > body.w)
        {
            y += dgl_round_real32_to_int32(font->height + font->linegap);
            current_w = 0;
            x = body.x;
        }
        // TODO(dgl): kinda ugly @@cleanup
        while((text_count > 0) && (word_byte_count > 0) && *c)
        {
            int32 char_bytes = utf8_to_codepoint(c, &codepoint);
            c += char_bytes;
            text_count -= char_bytes;
            word_byte_count -= char_bytes;

            if(codepoint == '\n')
            {
                y += dgl_round_real32_to_int32(font->height + font->linegap);
                current_w = 0;
                x = body.x;
                continue;
            }

            stbtt_bakedchar raw_glyph = font->glyphs[codepoint];

            V4 glyph = rect(raw_glyph.x0, raw_glyph.y0, raw_glyph.x1 - raw_glyph.x0, raw_glyph.y1 - raw_glyph.y0);
            V2 pos = v2(x + dgl_round_real32_to_int32(raw_glyph.xoff), y + dgl_round_real32_to_int32(raw_glyph.yoff));
            ren_draw_bitmap(ctx->buffer, font->bitmap, glyph, pos, color);

            int32 advance = dgl_round_real32_to_int32(raw_glyph.xadvance);
            current_w += advance;
            x += advance;
        }
    }

    V2 result = v2(body.w, y - body.y);
    return(result);
}

internal bool32
ui_button(Imui_Context *ctx, V4 body, V4 prim_color, V4 hover_color, Font *font, char *label)
{
    V4 label_color = ctx->fg_color;
    bool32 result = false;
    usize label_count = dgl_string_length(label);

    Element_ID id;
    if(label_count > 0)
    {
        id = get_id(ctx, label, label_count);
    }
    else
    {
        id = get_id(ctx, &body, sizeof(body));
    }
    result = begin_element(ctx, id, body);

    if(id == ctx->hot || id == ctx->active) { ren_draw_rectangle(ctx->buffer, body, hover_color); }
    else { ren_draw_rectangle(ctx->buffer, body, prim_color); }

    if(label_count > 0)
    {
        int32 width = get_font_width(font, label, dgl_safe_size_to_int32(label_count));
        char *c = label;
        uint32 codepoint = 0;
        int32 x = body.x + ((body.w - width) / 2);
        int32 y = body.y + ((body.h - dgl_round_real32_to_int32(font->height)) / 2);
        while(*c)
        {
            c += utf8_to_codepoint(c, &codepoint);

            if(codepoint == '\n') { continue; }

            stbtt_bakedchar raw_glyph = font->glyphs[codepoint];

            V4 glyph = rect(raw_glyph.x0, raw_glyph.y0, raw_glyph.x1 - raw_glyph.x0, raw_glyph.y1 - raw_glyph.y0);
            V2 pos = v2(x + dgl_round_real32_to_int32(raw_glyph.xoff), y + dgl_round_real32_to_int32(raw_glyph.yoff));
            ren_draw_bitmap(ctx->buffer, font->bitmap, glyph, pos, label_color);

            x += dgl_round_real32_to_int32(raw_glyph.xadvance);
        }
    }

    end_element(ctx);
    return(result);
}

internal void
ui_menu(Imui_Context *ctx, V4 body, V4 prim_col, V4 sec_col)
{
    V4 pad = {.top=20, .bottom=20, .right=20, .left=20};
    V2 button = {};
    button.w = (body.w - pad.left - 3*pad.right) / 3;
    button.h = body.h - pad.top - pad.bottom;
    int32 x = body.x + pad.left;
    int32 y = body.y + pad.top;

    V4 r = rect(x, y, button.w, button.h);
    if(ui_button(ctx, r, prim_col, sec_col, ctx->system_font, "X"))
    {
        V4 temp = ctx->fg_color;
        ctx->fg_color = ctx->bg_color;
        ctx->bg_color = temp;
    }
    x += button.w + pad.right;

    r = rect(x, y, button.w, button.h);
    if(ui_button(ctx, r, prim_col, sec_col, ctx->system_font, "V"))
    {
        ctx->desired_text_font_size = dgl_clamp(ctx->desired_text_font_size - 5.0f, 8.0f, 60.0f);
    }
    x += button.w + pad.right;

    r = rect(x, y, button.w, button.h);
    if(ui_button(ctx, r, prim_col, sec_col, ctx->system_font, "^"))
    {
        ctx->desired_text_font_size = dgl_clamp(ctx->desired_text_font_size + 5.0f, 8.0f, 60.0f);
    }
    x += button.w + pad.right;
}

internal void
ui_main_text(Imui_Context *ctx, File *file)
{
    V4 body = rect(50, 50, ctx->window.w - 100, ctx->window.h - 100);

    // NOTE(dgl): We use a hash of the data. Then every file has it's own state
    // and the scroll position is saved.
    Element_ID id = get_id(ctx, file->data, file->info->size);
    Element_State *c = get_element_state(ctx, id);

    begin_element(ctx, id, body);

    body.y -= c->scroll_pos;
    c->content = ui_textarea(ctx, ctx->text_font, body, ctx->fg_color, (char *)file->data, dgl_safe_size_to_int32(file->info->size));

    if(c->content.h > body.h)
    {
        int32 overflow = c->content.h - body.h;
        int32 scroll_delta = 0;
        if(ctx->input->scroll_delta.y != 0)
        {
            scroll_delta = -(ctx->input->scroll_delta.y * 30);
        }
        else if(ctx->active == id)
        {
            scroll_delta = ctx->input->last_pos.y - ctx->input->pos.y;
        }

        c->scroll_pos = dgl_clamp(c->scroll_pos + scroll_delta, 0, overflow);
    }
    else
    {
        c->scroll_pos = 0;
    }

    end_element(ctx);
}

void
zhc_update_and_render(Zhc_Memory *memory, Zhc_Input *input, Zhc_Offscreen_Buffer *buffer)
{
    assert(sizeof(Lib_State) < memory->storage_size, "Not enough memory allocated");
    platform = memory->api;

    Lib_State *state = cast(Lib_State *)memory->storage;
    if(!state->is_initialized)
    {
        LOG_DEBUG("Lib_State size: %lld, Available memory: %lld", sizeof(*state), memory->storage_size);
        dgl_mem_arena_init(&state->permanent_arena, (uint8 *)memory->storage + sizeof(*state), ((DGL_Mem_Index)memory->storage_size - sizeof(*state)));
        // TODO(dgl): create transient storage and put the initializing stuff there instead of a temp arena.

        // NOTE(dgl): Initializing UI Context
        Imui_Context *ui_ctx = dgl_mem_arena_push_struct(&state->permanent_arena, Imui_Context);
        ui_ctx->id_stack.count = 64; /* NOTE(dgl): Max count of elements. Increase if necessary */
        ui_ctx->id_stack.memory = dgl_mem_arena_push_array(&state->permanent_arena, Element_ID, ui_ctx->id_stack.count);
        ui_ctx->element_state_list.count = 64; /* NOTE(dgl): Max count of elements. Increase if necessary */
        ui_ctx->element_state_list.memory = dgl_mem_arena_push_array(&state->permanent_arena, Element_State, ui_ctx->element_state_list.count);

        // TODO(dgl): dgl stringbuilder add temp append and forbid resizing via flags
        DGL_String_Builder data_base_path_builder = dgl_string_builder_init(&state->permanent_arena, 512);
        bool32 base_path_success = platform.get_data_base_path(&data_base_path_builder);
        assert(base_path_success, "Could not load system path");
        dgl_string_append(&data_base_path_builder, "fonts/Inter-Regular.ttf");

        char *path = dgl_string_c_style(&data_base_path_builder);
        usize file_size = platform.file_size(path);
        uint8 *ttf_buffer = dgl_mem_arena_push_array(&state->permanent_arena, uint8, file_size);
        bool32 file_success = platform.read_entire_file(path, ttf_buffer, file_size);
        assert(file_success, "Could not load default font");

        LOG_DEBUG("Initializing font with buffer %p, size: %d", ttf_buffer, file_size);
        ui_ctx->system_font = initialize_font(&state->permanent_arena, ttf_buffer, 21.0f);

        usize font_arena_size = megabytes(8);
        uint8 *font_arena_base = dgl_mem_arena_push_array(&state->permanent_arena, uint8, font_arena_size);
        dgl_mem_arena_init(&ui_ctx->dyn_font_arena, font_arena_base, font_arena_size);

        // TODO(dgl): load from config
        ui_ctx->desired_text_font_size = 18.0f;
        ui_ctx->text_font = initialize_font(&ui_ctx->dyn_font_arena, ttf_buffer, ui_ctx->desired_text_font_size);
        ui_ctx->fg_color = color(0.0f, 0.0f, 0.0f, 1.0f);
        ui_ctx->bg_color = color(1.0f, 1.0f, 1.0f, 1.0f);

        state->ui_ctx = ui_ctx;

        // NOTE(dgl): Initialize IO Context
        // The io_arena must be initialized before beginnging the temp arena. Otherwise the allocation
        // is automatically freed on the end of the temp arena.
        usize io_arena_size = megabytes(8);
        uint8 *io_arena_base = dgl_mem_arena_push_array(&state->permanent_arena, uint8, io_arena_size);
        dgl_mem_arena_init(&state->io_arena, io_arena_base, io_arena_size);
        state->io_update_timeout = 0.0f;

        // TODO(dgl): let user set this folder and store in config
        DGL_Mem_Temp_Arena temp = dgl_mem_arena_begin_temp(&state->permanent_arena);
        DGL_String_Builder temp_builder = dgl_string_builder_init(temp.arena, 128);

        if(platform.get_user_data_base_path(&temp_builder))
        {
            char *temp_target = dgl_string_c_style(&temp_builder);
            Zhc_File_Group *group = platform.get_directory_filenames(&state->io_arena, temp_target);

            // TODO(dgl): order filegroup by filename. Maybe order on list creation.

            if(group)
            {
                state->files = group;

                Zhc_File_Info *info = get_file_info(group, 0);
                if(info)
                {
                    state->active_file = read_active_file(&state->io_arena, group, info);
                }
            }
        }

        dgl_mem_arena_end_temp(temp);
        state->is_initialized = true;
    }

    Imui_Context *ui_ctx = state->ui_ctx;
    ui_ctx->input = input;
    ui_ctx->buffer = buffer;
    ui_ctx->window.x = buffer->width;
    ui_ctx->window.y = buffer->height;
    // NOTE(dgl): if the hot element id did not get updated on the last frame, we reset the id.
    if(!ui_ctx->hot_updated) { ui_ctx->hot = 0; }
    ui_ctx->hot_updated = false;

    // NOTE(dgl): draw backplate
    V4 screen = rect(0, 0, ui_ctx->window.w, ui_ctx->window.h);
    ren_draw_rectangle(ui_ctx->buffer, screen, ui_ctx->bg_color);

    // NOTE(dgl): draw file, if it is available
    if(state->active_file.data)
    {
        ui_main_text(ui_ctx, &state->active_file);
    }

    ui_menu(ui_ctx,
            rect(ui_ctx->window.w - 300, 0, 300, 100),
            color(ui_ctx->fg_color.r, ui_ctx->fg_color.g, ui_ctx->fg_color.b, 0.025f),
            color(ui_ctx->fg_color.r, ui_ctx->fg_color.g, ui_ctx->fg_color.b, 0.5f));

    // TODO(dgl): use command buffer instead of desired file etc..
    int32 button_w = 100;
    int32 button_h = 400;
    if(ui_button(ui_ctx,
                 rect(ui_ctx->window.w - button_w, (ui_ctx->window.h - button_h)/2, button_w, button_h),
                 color(ui_ctx->fg_color.r, ui_ctx->fg_color.g, ui_ctx->fg_color.b, 0.025f),
                 color(ui_ctx->fg_color.r, ui_ctx->fg_color.g, ui_ctx->fg_color.b, 0.5f),
                 ui_ctx->system_font, ">") ||
       input_pressed(ui_ctx->input, Zhc_Keyboard_Button_Right) ||
       input_pressed(ui_ctx->input, Zhc_Keyboard_Button_Enter) ||
       input_pressed(ui_ctx->input, ' '))
    {
        if(state->files)
        {
            ui_ctx->desired_file_id = dgl_clamp(ui_ctx->desired_file_id + 1, 0, state->files->count - 1);
        }
    }

    if(ui_button(ui_ctx,
                 rect(0, (ui_ctx->window.h - button_h)/2, button_w, button_h),
                 color(ui_ctx->fg_color.r, ui_ctx->fg_color.g, ui_ctx->fg_color.b, 0.025f),
                 color(ui_ctx->fg_color.r, ui_ctx->fg_color.g, ui_ctx->fg_color.b, 0.5f),
                 ui_ctx->system_font, "<") ||
       input_pressed(ui_ctx->input, Zhc_Keyboard_Button_Left))
    {
        if(state->files)
        {
            ui_ctx->desired_file_id = dgl_clamp(ui_ctx->desired_file_id - 1, 0, state->files->count - 1);
        }
    }

    // NOTE(dgl): Reload the directory/file info and file every 10 seconds.
    state->io_update_timeout += input->last_frame_in_ms;
    if(state->io_update_timeout > 10000.0f && state->files->count > 0)
    {
        DGL_Mem_Temp_Arena temp = dgl_mem_arena_begin_temp(&state->permanent_arena);

        // NOTE(dgl): copy current infos to have them available after
        // puring the io arena. // TODO(dgl): Is there a better way?
        DGL_String_Builder tmp_builder = dgl_string_builder_init(temp.arena, 128);
        dgl_string_append(&tmp_builder, "%s", state->files->dirpath);
        char *tmp_dir_path = dgl_string_c_style(&tmp_builder);

        // NOTE(dgl): resetting pointer to make sure, we do not point to something invalid
        state->active_file = {};
        state->files = 0;
        dgl_mem_arena_free_all(&state->io_arena);

        state->io_update_timeout = 0.0f;

        Zhc_File_Group *group = platform.get_directory_filenames(&state->io_arena, tmp_dir_path);
        if(group)
        {
            state->files = group;
            Zhc_File_Info *info = get_file_info(state->files, state->ui_ctx->desired_file_id);
            // NOTE(dgl): we resetted the file earlier. Therefore we do not need to reset if
            // the file does not exist anymore.
            if(info)
            {
                state->active_file = read_active_file(&state->io_arena, state->files, info);
            }
        }

        dgl_mem_arena_end_temp(temp);
    }

    // NOTE(dgl): update font size, if requested
    if(ui_ctx->desired_text_font_size != ui_ctx->text_font->size)
    {
        LOG_DEBUG("Resizing font from %f to %f", ui_ctx->text_font->size, ui_ctx->desired_text_font_size);
        dgl_mem_arena_free_all(&ui_ctx->dyn_font_arena);
        ui_ctx->text_font = initialize_font(&ui_ctx->dyn_font_arena, ui_ctx->text_font->ttf_buffer, ui_ctx->desired_text_font_size);
    }

    // NOTE(dgl): update active file if requested
    Zhc_File_Info *info = get_file_info(state->files, state->ui_ctx->desired_file_id);
    if(info && info != state->active_file.info)
    {
        state->active_file = read_active_file(&state->io_arena, state->files, info);
    }

    // NOTE(dgl): put this at the end of the frame
    // to know which element is hot if they are overlapping
    // on the next frame
    ui_ctx->top_most_hot = ui_ctx->hot;
}
