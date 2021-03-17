// NOTE(dgl): em is based on the current font size of if not available 16px
internal inline int32
em(Theme theme, real32 value)
{
    int32 result = 0;
    if(theme.font_size)
    {
        result = cast(int32)(cast(real32)theme.font_size * value + 0.5f);
    }
    else
    {
        result = cast(int32)(value * 16.0f + 0.5f);
    }
    return(result);
}

internal inline int32
em(Imui_Context *ctx, real32 value)
{
    Theme theme = get_default_theme(ctx->screen);
    return(em(theme, value));
}

internal Theme
default_theme_xs()
{
    Theme result = {};
    result.font_size = 21;
    result.icon_size = 64;
    result.menu_size = { .w=em(result, 16.0f), .h=em(result, 6.0f)};
    result.primary_color = color(0.1f, 0.1f, 0.1f, 1.0f);
    result.bg_color = color(1.0f, 1.0f, 1.0f, 1.0f);

    return(result);
}

internal Theme
default_theme_sm()
{
    Theme result = {};
    result.font_size = 21;
    result.icon_size = 64;
    result.menu_size = { .w=em(result, 16.0f), .h=em(result, 6.0f)};
    result.primary_color = color(0.1f, 0.1f, 0.1f, 1.0f);
    result.bg_color = color(1.0f, 1.0f, 1.0f, 1.0f);

    return(result);
}

internal Theme
default_theme_md()
{
    Theme result = {};
    result.font_size = 18;
    result.icon_size = 32;
    result.menu_size = { .w=em(result, 14.0f), .h=em(result, 5.0f)};
    result.primary_color = color(0.1f, 0.1f, 0.1f, 1.0f);
    result.bg_color = color(1.0f, 1.0f, 1.0f, 1.0f);

    return(result);
}

internal Theme
default_theme_lg()
{
    Theme result = {};
    result.font_size = 21;
    result.icon_size = 32;
    result.menu_size = { .w=em(result, 15.0f), .h=em(result, 5.0f)};
    result.primary_color = color(0.1f, 0.1f, 0.1f, 1.0f);
    result.bg_color = color(1.0f, 1.0f, 1.0f, 1.0f);

    return(result);
}

internal Theme
default_theme_xl()
{
    Theme result = {};
    result.font_size = 24;
    result.icon_size = 32;
    result.menu_size = { .w=em(result, 15.0f), .h=em(result, 5.0f)};
    result.primary_color = color(0.1f, 0.1f, 0.1f, 1.0f);
    result.bg_color = color(1.0f, 1.0f, 1.0f, 1.0f);

    return(result);
}

internal Theme
get_default_theme(Screen_Size screen)
{
    Theme result = {};

    switch(screen)
    {
        case Screen_Size_XL: result = default_theme_xl(); break;
        case Screen_Size_LG: result = default_theme_lg(); break;
        case Screen_Size_MD: result = default_theme_md(); break;
        case Screen_Size_SM: result = default_theme_sm(); break;
        default: result = default_theme_xs();
    }

    return(result);
}

internal Button_Theme
default_button_theme(Imui_Context *ctx)
{
    Button_Theme result = {};

    Theme default_theme = get_default_theme(ctx->screen);

    result.icon_color = default_theme.primary_color;
    result.icon_size = default_theme.icon_size;
    result.hover_color = default_theme.primary_color;
    result.hover_color.a = 0.1f;
    result.bg_color = color(default_theme.bg_color.r, default_theme.bg_color.g, default_theme.bg_color.b, 0.3f);
    result.bg_color.a = 0.3f;

    if(ctx->is_dark)
    {
        result.icon_color = default_theme.bg_color;

        V4 tmp = result.bg_color;
        result.bg_color = result.hover_color;
        result.hover_color = tmp;

        result.hover_color.a = 0.1f;
        result.bg_color.a = 0.3f;
    }

    return(result);
}

internal bool32
maybe_update_screen_size(Imui_Context *ctx)
{
    bool32 result = false;
    Screen_Size screen = Screen_Uninitialized;
    if(ctx->window.w >= Screen_Size_XL) { screen = Screen_Size_XL; }
    else if(ctx->window.w >= Screen_Size_LG) { screen = Screen_Size_LG; }
    else if(ctx->window.w >= Screen_Size_MD) { screen = Screen_Size_MD; }
    else if(ctx->window.w >= Screen_Size_SM) { screen = Screen_Size_SM; }
    else { screen = Screen_Size_XS; }

    if(screen != ctx->screen)
    {
        ctx->screen = screen;
        result = true;
    }

    return(result);
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

internal inline stbtt_bakedchar
get_font_glyph(Loaded_Font *font, uint32 codepoint)
{
    stbtt_bakedchar result = {};
    if(codepoint >= font->glyph_count)
    {
        LOG_DEBUG("Glyph cannot be drawn. We currently support only %d glyphs", font->glyph_count);
        codepoint = 0;
    }

    result = font->glyphs[codepoint];
    return(result);
}

internal int32
get_font_width(Loaded_Font *font, char *text, int32 byte_count)
{
    char *c = text;
    int32 result = 0;
    uint32 codepoint = 0;
    while(*c && (byte_count-- > 0))
    {
        c += utf8_to_codepoint(c, &codepoint);
        stbtt_bakedchar glyph = get_font_glyph(font, codepoint);
        result += dgl_round_real32_to_int32(glyph.xadvance);
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

internal void
ui_icon(Imui_Context *ctx, Icon_Type type, int32 size, V2 pos, V4 color)
{
    assert(type < Icon_Type_Count, "Count is not an icon type");

    int32 index = 0;
    Icon_Set *set = 0;
    while(index < array_count(ctx->icon_sets))
    {
        Icon_Set *tmp = ctx->icon_sets + index;
        if(tmp->size == size)
        {
            set = tmp;
            break;
        }
        ++index;
    }

    assert(set, "Icon size is not available.");

    Icon *icon = set->icons + type;
    Loaded_Image *img = assets_get_image(ctx->assets, icon->bitmap);
    ren_draw_bitmap(ctx->buffer, img, icon->box, pos, color);
}

// NOTE(dgl): width and height of the element (to calculate overflow)
internal V2
ui_textarea(Imui_Context *ctx, Font *font, V4 body, V4 color, char* text, int32 text_count)
{
    Loaded_Image *font_bitmap = assets_get_image(ctx->assets, font->bitmap);
    Loaded_Font *font_asset = assets_get_font(ctx->assets, font->font_asset);

    assert(font_bitmap, "Initialize font before rendering text");
    assert(font_asset, "Initialize font before rendering text");

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
        int32 word_width = get_font_width(font_asset, c, word_byte_count);

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

            stbtt_bakedchar raw_glyph = get_font_glyph(font_asset, codepoint);

            V4 glyph = rect(raw_glyph.x0, raw_glyph.y0, raw_glyph.x1 - raw_glyph.x0, raw_glyph.y1 - raw_glyph.y0);
            V2 pos = v2(x + dgl_round_real32_to_int32(raw_glyph.xoff), y + dgl_round_real32_to_int32(raw_glyph.yoff));
            ren_draw_bitmap(ctx->buffer, font_bitmap, glyph, pos, color);

            int32 advance = dgl_round_real32_to_int32(raw_glyph.xadvance);
            current_w += advance;
            x += advance;
        }
    }

    V2 result = v2(body.w, y - body.y);
    return(result);
}

internal bool32
button(Imui_Context *ctx, V4 body, Button_Theme theme, Icon_Type type)
{
    bool32 result = false;

    Element_ID id = get_id(ctx, &body, sizeof(body));;
    result = begin_element(ctx, id, body);

    if(id == ctx->hot || id == ctx->active) { ren_draw_rectangle(ctx->buffer, body, theme.hover_color); }
    else { ren_draw_rectangle(ctx->buffer, body, theme.bg_color); }

    V2 icon_pos = v2(body.x + (body.w - theme.icon_size) / 2, body.y + (body.h - theme.icon_size) / 2);

    ui_icon(ctx, type, theme.icon_size, icon_pos, theme.icon_color);

    end_element(ctx);
    return(result);
}

// NOTE(dgl): try to resize only fonts which are the last allocation in the
// arena for easier allocation resizing.
internal void
ui_resize_font(Zhc_Assets *assets, Font *font, int32 font_size)
{
    assert(font, "Font cannot be null");
    Loaded_Font *font_asset = assets_get_font(assets, font->font_asset);

    // Get font metrics
    int32 ascent, descent, linegap;
    stbtt_GetFontVMetrics(&font_asset->stbfont, &ascent, &descent, &linegap);

    font->size = font_size;
    real32 scale = stbtt_ScaleForPixelHeight(&font_asset->stbfont, cast(real32)font->size);
    // NOTE(dgl): linegap is defined by the font. However it was 0 in the fonts I
    // have tested.
    font->linegap = 1.2f; //cast(real32)linegap;
    font->height = cast(real32)(ascent - descent) * scale;

    // NOTE(dgl): loading the font_bitmap, if it exists. Otherwise this pointer is NULL!
    Loaded_Image *font_bitmap = assets_get_image(assets, font->bitmap);
    int32 bitmap_width = 128;
    int32 bitmap_height = 128;

retry:
    int32 pixel_count = bitmap_width * bitmap_height;
    if(font_bitmap)
    {
        LOG_DEBUG("Unloading bitmap buffer (%dx%d) for resizing", bitmap_width, bitmap_height);
        assets_unload(assets, font->bitmap);
    }

    LOG_DEBUG("Loading bitmap buffer %dx%d", bitmap_width, bitmap_height);
    assets_allocate_image(assets, font->bitmap, bitmap_width, bitmap_height);
    font_bitmap = assets_get_image(assets, font->bitmap);

    real32 s = stbtt_ScaleForMappingEmToPixels(&font_asset->stbfont, 1) / stbtt_ScaleForPixelHeight(&font_asset->stbfont, 1);

    /* load glyphs */
    int32 success = stbtt_BakeFontBitmap(font_asset->ttf_buffer, 0, cast(real32)font->size * s,
                                         cast(uint8 *)font_bitmap->pixels, font_bitmap->width, font_bitmap->height,
                                         0, font_asset->glyph_count, font_asset->glyphs);

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
    font_asset->glyphs[cast(int32)'\t'].x1 = font_asset->glyphs[cast(int32)'\t'].x0;
    font_asset->glyphs[cast(int32)'\n'].x1 = font_asset->glyphs[cast(int32)'\n'].x0;
}

internal Zhc_File_Group *
find_assets(DGL_Mem_Arena *arena, char *asset_path)
{
    Zhc_File_Group *result = 0;
    DGL_String_Builder tmp_path = dgl_string_builder_init(arena, 256);
    bool32 path_success = platform.get_data_base_path(&tmp_path);
    assert(path_success, "Could not load system path");

    dgl_string_append(&tmp_path, asset_path);
    result = platform.get_directory_filenames(arena, dgl_string_c_style(&tmp_path));

    return(result);
}

internal Icon_Set
initialize_icons(Zhc_Assets *assets, Zhc_File_Info *info, int32 size)
{
    Icon_Set result = {};
    result.size = size;
    Asset_ID bitmap_id = assets_push_file(assets, info->handle, info->size);

    for(int32 index = 0; index < Icon_Type_Count; ++index)
    {
        Icon *icon = result.icons + index;
        icon->bitmap = bitmap_id;
        icon->type = cast(Icon_Type)index; // NOTE(dgl): index is here mapped to icon_type
        icon->box = rect(index * size, 0, size, size);
    }

    return(result);
}

void
ui_draw_menu(Imui_Context *ctx)
{
    Theme default_theme = get_default_theme(ctx->screen);
    V4 body = rect(ctx->window.w - default_theme.menu_size.x, 0, default_theme.menu_size.x, default_theme.menu_size.y);
    V4 pad = {.top=20, .bottom=20, .right=20, .left=20};
    V2 button_body = {};
    button_body.w = (body.w - pad.left - 3*pad.right) / 3;
    button_body.h = body.h - pad.top - pad.bottom;
    int32 x = body.x + pad.left;
    int32 y = body.y + pad.top;

    Button_Theme button_theme = default_button_theme(ctx);

    V4 r = rect(x, y, button_body.w, button_body.h);
    Icon_Type mode_swap = Icon_Type_Dark;
    if(ctx->is_dark) { mode_swap = Icon_Type_Light; }
    if(button(ctx, r, button_theme, mode_swap))
    {
        ctx->is_dark = !ctx->is_dark;
    }
    x += button_body.w + pad.right;

    int32 new_font_size = 0;
    r = rect(x, y, button_body.w, button_body.h);
    if(button(ctx, r, button_theme, Icon_Type_Decrease_Font))
    {
        new_font_size = ctx->text_font.size - em(ctx, 0.3f);
    }
    x += button_body.w + pad.right;

    r = rect(x, y, button_body.w, button_body.h);
    if(button(ctx, r, button_theme, Icon_Type_Increase_Font))
    {
        new_font_size = ctx->text_font.size + em(ctx, 0.3f);
    }
    x += button_body.w + pad.right;

    if(new_font_size > 0)
    {
        int32 clamped = dgl_clamp(new_font_size, em(ctx, 0.5f), MAX_FONT_SIZE);
        LOG_DEBUG("Resizing font from %d to %d", ctx->text_font.size, clamped);
        ui_resize_font(ctx->assets, &ctx->text_font, clamped);
    }
}

void
ui_draw_file_controls(Imui_Context *ctx, int32 *file_index)
{
    Button_Theme theme = default_button_theme(ctx);
    theme.icon_size = 64;
    int32 button_w = 100;
    int32 button_h = 400;

    if(button(ctx,
              rect(ctx->window.w - button_w, (ctx->window.h - button_h)/2, button_w, button_h),
              theme,
              Icon_Type_Next) ||
              input_pressed(ctx->input, Zhc_Keyboard_Button_Right) ||
              input_pressed(ctx->input, Zhc_Keyboard_Button_Enter) ||
              input_pressed(ctx->input, ' '))
    {
        (*file_index)++;
    }

    if(button(ctx,
              rect(0, (ctx->window.h - button_h)/2, button_w, button_h),
              theme,
              Icon_Type_Previous) ||
              input_pressed(ctx->input, Zhc_Keyboard_Button_Left))
    {
        (*file_index)--;
    }
}

void
ui_draw_textarea(Imui_Context *ctx, char *text, usize text_count)
{
    Theme default_theme = get_default_theme(ctx->screen);

    V4 pad = {.top=default_theme.icon_size + 20, .bottom=dgl_round_real32_to_int32(ctx->text_font.height) + 100, .right=50, .left=50};
    V4 body = rect(pad.left, pad.top, ctx->window.w - pad.right, ctx->window.h - pad.bottom);

    // NOTE(dgl): We use a hash of the data. Then every file has it's own state
    // and the scroll position is saved.
    Element_ID id = get_id(ctx, text, text_count);
    Element_State *c = get_element_state(ctx, id);

    begin_element(ctx, id, body);

    body.y -= c->scroll_pos;

    V4 text_color = default_theme.primary_color;
    if(ctx->is_dark) { text_color = default_theme.bg_color; }
    c->content = ui_textarea(ctx, &ctx->text_font, body, text_color, text, dgl_safe_size_to_int32(text_count));

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
ui_draw_backplate(Imui_Context *ctx)
{
    V4 screen = rect(0, 0, ctx->window.w, ctx->window.h);

    Theme default_theme = get_default_theme(ctx->screen);
    V4 bg_color = default_theme.bg_color;
    if(ctx->is_dark) { bg_color = default_theme.primary_color; }
    ren_draw_rectangle(ctx->buffer, screen, bg_color);
}

Imui_Context *
ui_context_init(DGL_Mem_Arena *permanent_arena, DGL_Mem_Arena *transient_arena)
{
    LOG_DEBUG("Permanent arena %p, Transient arena %p", permanent_arena, transient_arena);
    Imui_Context *result = dgl_mem_arena_push_struct(permanent_arena, Imui_Context);
    result->id_stack.count = 64; /* NOTE(dgl): Max count of elements. Increase if necessary */
    result->id_stack.memory = dgl_mem_arena_push_array(permanent_arena, Element_ID, result->id_stack.count);
    result->element_state_list.count = 64; /* NOTE(dgl): Max count of elements. Increase if necessary */
    result->element_state_list.memory = dgl_mem_arena_push_array(permanent_arena, Element_State, result->element_state_list.count);

    result->assets = assets_begin_allocate(permanent_arena, megabytes(24));
    {
        // NOTE(dgl): initializing fonts
        // TODO(dgl): @here opendir does not find the directories.
        Zhc_File_Group *font_group = find_assets(transient_arena, "fonts");
        if(font_group)
        {
            assert(font_group->count >= 1, "No font file found");
            result->system_font.font_asset = assets_push_file(result->assets, font_group->first_file_info->handle, font_group->first_file_info->size);
            // TODO(dgl): make it possible to update the asset_file info to change font files, e.g. for network load?
            result->text_font.font_asset = assets_push_file(result->assets, font_group->first_file_info->handle, font_group->first_file_info->size);

            result->system_font.bitmap = assets_push(result->assets);
            result->text_font.bitmap = assets_push(result->assets);
        }

        // NOTE(dgl): initializing icons
        Zhc_File_Group *icon_group = find_assets(transient_arena, "images");
        if(icon_group)
        {
            Zhc_File_Info *icon_set = icon_group->first_file_info;

            int32 index = 0;
            while(icon_set && index < array_count(result->icon_sets))
            {
                LOG_DEBUG("Filename %s", icon_set->filename);
                // TODO(dgl): @@performance But only executed once during init.
                if(strcmp(icon_set->filename, "16x16.png") == 0)
                {
                    result->icon_sets[index++] = initialize_icons(result->assets, icon_set, 16);
                }
                else if(strcmp(icon_set->filename, "24x24.png") == 0)
                {
                    result->icon_sets[index++] = initialize_icons(result->assets, icon_set, 24);
                }
                else if(strcmp(icon_set->filename, "32x32.png") == 0)
                {
                    result->icon_sets[index++] = initialize_icons(result->assets, icon_set, 32);
                }
                else if(strcmp(icon_set->filename, "64x64.png") == 0)
                {
                    result->icon_sets[index++] = initialize_icons(result->assets, icon_set, 64);
                }
                else if(strcmp(icon_set->filename, "128x128.png") == 0)
                {
                    result->icon_sets[index++] = initialize_icons(result->assets, icon_set, 128);
                }

                icon_set = icon_set->next;
            }
        }
    }
    assets_end_allocate(result->assets);
    LOG_DEBUG("End allocating assets");

    ui_resize_font(result->assets, &result->system_font, em(result, 1));
    ui_resize_font(result->assets, &result->text_font, em(result, 1));

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

    // NOTE(dgl): update theme if the screen size has changed from desktop to e.g. mobile
    if(maybe_update_screen_size(ctx))
    {
        Theme default_theme = get_default_theme(ctx->screen);
        ui_resize_font(ctx->assets, &ctx->system_font, default_theme.font_size);

        // NOTE(dgl): unload all assets not needed for this theme. If sizes are still needed,
        // we reload them on demand @@performance
        for(int32 index = 0; index < array_count(ctx->icon_sets); ++index)
        {
            Icon_Set *set = ctx->icon_sets + index;
            if(set->size == default_theme.icon_size) { continue; }
            assets_unload(ctx->assets, set->icons[0].bitmap);
        }
    }
}
