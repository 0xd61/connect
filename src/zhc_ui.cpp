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
ui_main_text(Imui_Context *ctx, char *text, usize text_count)
{
    V4 body = rect(50, 50, ctx->window.w - 100, ctx->window.h - 100);

    // NOTE(dgl): We use a hash of the data. Then every file has it's own state
    // and the scroll position is saved.
    Element_ID id = get_id(ctx, text, text_count);
    Element_State *c = get_element_state(ctx, id);

    begin_element(ctx, id, body);

    body.y -= c->scroll_pos;
    c->content = ui_textarea(ctx, ctx->text_font, body, ctx->fg_color, text, dgl_safe_size_to_int32(text_count));

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

Imui_Context *
ui_context_init(DGL_Mem_Arena *arena)
{
    Imui_Context *result = dgl_mem_arena_push_struct(arena, Imui_Context);
    result->id_stack.count = 64; /* NOTE(dgl): Max count of elements. Increase if necessary */
    result->id_stack.memory = dgl_mem_arena_push_array(arena, Element_ID, result->id_stack.count);
    result->element_state_list.count = 64; /* NOTE(dgl): Max count of elements. Increase if necessary */
    result->element_state_list.memory = dgl_mem_arena_push_array(arena, Element_State, result->element_state_list.count);

    // TODO(dgl): dgl stringbuilder add temp append and forbid resizing via flags
    DGL_String_Builder data_base_path_builder = dgl_string_builder_init(arena, 512);
    bool32 base_path_success = platform.get_data_base_path(&data_base_path_builder);
    assert(base_path_success, "Could not load system path");
    dgl_string_append(&data_base_path_builder, "fonts/Inter-Regular.ttf");

    char *path = dgl_string_c_style(&data_base_path_builder);
    usize file_size = platform.file_size(path);
    uint8 *ttf_buffer = dgl_mem_arena_push_array(arena, uint8, file_size);
    bool32 file_success = platform.read_entire_file(path, ttf_buffer, file_size);
    assert(file_success, "Could not load default font");

    LOG_DEBUG("Initializing font with buffer %p, size: %d", ttf_buffer, file_size);
    result->system_font = initialize_font(arena, ttf_buffer, 21.0f);

    usize font_arena_size = megabytes(8);
    uint8 *font_arena_base = dgl_mem_arena_push_array(arena, uint8, font_arena_size);
    dgl_mem_arena_init(&result->dyn_font_arena, font_arena_base, font_arena_size);

    // TODO(dgl): load from config
    result->desired_text_font_size = 18.0f;
    result->text_font = initialize_font(&result->dyn_font_arena, ttf_buffer, result->desired_text_font_size);
    result->fg_color = color(0.0f, 0.0f, 0.0f, 1.0f);
    result->bg_color = color(1.0f, 1.0f, 1.0f, 1.0f);

    return(result);
}

void
ui_context_update(Imui_Context *ctx, Zhc_Input *input, Zhc_Offscreen_Buffer *buffer)
{
    ctx->input = input;
    ctx->buffer = buffer;
    ctx->window.x = buffer->width;
    ctx->window.y = buffer->height;

    // NOTE(dgl): if the hot element id did not get updated on the last frame, we reset the id.
    if(!ctx->hot_updated) { ctx->hot = 0; }
    ctx->hot_updated = false;
}
