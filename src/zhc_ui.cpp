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
    result.font_size = 26;
    result.icon_size = 48;
    result.menu_size = { .w=em(result, 10.0f), .h=em(result, 4.0f)};
    result.primary_color = v4(0.1f, 0.1f, 0.1f, 1.0f);
    result.bg_color = v4(1.0f, 1.0f, 1.0f, 1.0f);

    return(result);
}

internal Theme
default_theme_sm()
{
    Theme result = {};
    result.font_size = 32;
    result.icon_size = 64;
    result.menu_size = { .w=em(result, 10.0f), .h=em(result, 3.5f)};
    result.primary_color = v4(0.1f, 0.1f, 0.1f, 1.0f);
    result.bg_color = v4(1.0f, 1.0f, 1.0f, 1.0f);

    return(result);
}

internal Theme
default_theme_md()
{
    Theme result = {};
    result.font_size = 46;
    result.icon_size = 64;
    result.menu_size = { .w=em(result, 10.0f), .h=em(result, 4.0f)};
    result.primary_color = v4(0.1f, 0.1f, 0.1f, 1.0f);
    result.bg_color = v4(1.0f, 1.0f, 1.0f, 1.0f);

    return(result);
}

internal Theme
default_theme_lg()
{
    Theme result = {};
    result.font_size = 52;
    result.icon_size = 96;
    result.menu_size = { .w=em(result, 11.0f), .h=em(result, 4.0f)};
    result.primary_color = v4(0.1f, 0.1f, 0.1f, 1.0f);
    result.bg_color = v4(1.0f, 1.0f, 1.0f, 1.0f);

    return(result);
}

internal Theme
default_theme_xl()
{
    Theme result = {};
    result.font_size = 60;
    result.icon_size = 96;
    result.menu_size = { .w=em(result, 10.0f), .h=em(result, 5.0f)};
    result.primary_color = v4(0.1f, 0.1f, 0.1f, 1.0f);
    result.bg_color = v4(1.0f, 1.0f, 1.0f, 1.0f);

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
    result.bg_color = v4(default_theme.bg_color.r, default_theme.bg_color.g, default_theme.bg_color.b, 0.3f);
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
    if(ctx->input->window_dim.w >= Screen_Size_XL) { screen = Screen_Size_XL; }
    else if(ctx->input->window_dim.w >= Screen_Size_LG) { screen = Screen_Size_LG; }
    else if(ctx->input->window_dim.w >= Screen_Size_MD) { screen = Screen_Size_MD; }
    else if(ctx->input->window_dim.w >= Screen_Size_SM) { screen = Screen_Size_SM; }
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

internal void
push_text_render_command(Render_Command_Buffer *buffer, Font *font, uint32 codepoint, V2 pos, V4 color)
{
    Render_Command_Font *cmd = render_command_alloc(buffer, Render_Command_Type_Font, Render_Command_Font);
    cmd->pos = pos;
    cmd->color = color;
    cmd->font = font->font_asset;
    cmd->bitmap = font->bitmap;
    cmd->size = font->size;
    cmd->codepoint = codepoint;
}

internal void
push_image_render_command(Render_Command_Buffer *buffer, Asset_ID image, V2 pos, V4 rect, V4 color)
{
    Render_Command_Image *cmd = render_command_alloc(buffer, Render_Command_Type_Image, Render_Command_Image);
    cmd->pos = pos;
    cmd->rect = rect;
    cmd->color = color;
    cmd->image = image;
}

internal void
push_rect_render_command(Render_Command_Buffer *buffer, V4 rect, V4 color)
{
    Render_Command_Rect *cmd = render_command_alloc(buffer, Render_Command_Type_Rect_Filled, Render_Command_Rect);
    cmd->rect = rect;
    cmd->color = color;
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

internal void
recalculate_font_metrics(Zhc_Assets *assets, Font *font, int32 font_size)
{
    Loaded_Font *font_asset = assets_get_font(assets, font->font_asset);
    // Get font metrics
    int32 ascent, descent, linegap;
    stbtt_GetFontVMetrics(&font_asset->stbfont, &ascent, &descent, &linegap);

    real32 scale = stbtt_ScaleForPixelHeight(&font_asset->stbfont, cast(real32)font_size);
    // NOTE(dgl): linegap is defined by the font. However it was 0 in the fonts I
    // have tested.
    font->linegap = 1.4f; //cast(real32)linegap;
    font->height = cast(real32)(ascent - descent) * scale;
    font->size = font_size;
    font->mapping_scale = scale;
}

// TODO(dgl): @naming
internal real32
get_glyph_advance_width(Zhc_Assets *assets, Font *font, uint32 codepoint)
{
    real32 result = 0;
    assert(font->mapping_scale > 0, "Font is not initialized. Font metrics must be recalculated");

    Loaded_Font *font_asset = assets_get_font(assets, font->font_asset);

    int32 advance, lsb;
    // NOTE(dgl): necessary? It's unlikely a codepoint would be this big. An error value however could and we would catch
    // it here.
    assert(codepoint <= 0x7FFFFFFF, "Codepoint cannot be converted to int32. Too large.");
    stbtt_GetCodepointHMetrics(&font_asset->stbfont, cast(int32)codepoint, &advance, &lsb);

    result = cast(real32)advance * font->mapping_scale;

    return(result);
}

internal int32
get_text_width(Zhc_Assets *assets, Font *font, char *text, int32 byte_count)
{
    char *c = text;
    int32 result = 0;
    uint32 codepoint = 0;
    while(*c && (byte_count-- > 0))
    {
        c += utf8_to_codepoint(c, &codepoint);
        real32 advance = get_glyph_advance_width(assets, font, codepoint);
        result += dgl_round_real32_to_int32(advance);
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
    push_image_render_command(ctx->cmd_buffer, icon->bitmap, pos, icon->box, color);
}

// NOTE(dgl): width and height of the element (to calculate overflow)
// TODO(dgl): use textarena theme...
internal V2
ui_textarea(Imui_Context *ctx, Font *font, V4 body, V4 color, char* text, int32 text_count)
{
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
        int32 word_width = get_text_width(ctx->assets, font, c, word_byte_count);

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

            V2 pos = {.x=x, .y=y};
            push_text_render_command(ctx->cmd_buffer, font, codepoint, pos, color);

            int32 advance = dgl_round_real32_to_int32(get_glyph_advance_width(ctx->assets, font, codepoint));
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

    if(id == ctx->hot || id == ctx->active) { push_rect_render_command(ctx->cmd_buffer, body, theme.hover_color); }
    else { push_rect_render_command(ctx->cmd_buffer, body, theme.bg_color); }

    V2 icon_pos = v2(body.x + (body.w - theme.icon_size) / 2, body.y + (body.h - theme.icon_size) / 2);

    ui_icon(ctx, type, theme.icon_size, icon_pos, theme.icon_color);

    end_element(ctx);
    return(result);
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
        icon->box = v4(index * size, 0, size, size);
    }

    return(result);
}

void
ui_draw_menu(Imui_Context *ctx)
{
    Theme default_theme = get_default_theme(ctx->screen);
    V4 body = v4(ctx->input->window_dim.w - default_theme.menu_size.x, 0, default_theme.menu_size.x, default_theme.menu_size.y);
    V4 pad = {.top=20, .bottom=20, .right=20, .left=20};
    V2 button_body = {};
    button_body.w = (body.w - pad.left - 3*pad.right) / 3;
    button_body.h = body.h - pad.top - pad.bottom;
    int32 x = body.x + pad.left;
    int32 y = body.y + pad.top;

    Button_Theme button_theme = default_button_theme(ctx);

    V4 r = v4(x, y, button_body.w, button_body.h);
    Icon_Type mode_swap = Icon_Type_Dark;
    if(ctx->is_dark) { mode_swap = Icon_Type_Light; }
    if(button(ctx, r, button_theme, mode_swap))
    {
        ctx->is_dark = !ctx->is_dark;
    }
    x += button_body.w + pad.right;

    int32 new_font_size = 0;
    r = v4(x, y, button_body.w, button_body.h);
    if(button(ctx, r, button_theme, Icon_Type_Decrease_Font))
    {
        new_font_size = ctx->text_font.size - em(ctx, 0.3f);
    }
    x += button_body.w + pad.right;

    r = v4(x, y, button_body.w, button_body.h);
    if(button(ctx, r, button_theme, Icon_Type_Increase_Font))
    {
        new_font_size = ctx->text_font.size + em(ctx, 0.3f);
    }
    x += button_body.w + pad.right;

    if(new_font_size > 0)
    {
        int32 clamped = dgl_clamp(new_font_size, em(ctx, 0.5f), MAX_FONT_SIZE);
        LOG_DEBUG("Resizing font from %d to %d", ctx->text_font.size, clamped);
        recalculate_font_metrics(ctx->assets, &ctx->text_font, clamped);
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
              v4(ctx->input->window_dim.w - button_w, (ctx->input->window_dim.h - button_h)/2, button_w, button_h),
              theme,
              Icon_Type_Next) ||
              input_pressed(ctx->input, Zhc_Keyboard_Button_Right) ||
              input_pressed(ctx->input, Zhc_Keyboard_Button_Enter) ||
              input_pressed(ctx->input, ' '))
    {
        (*file_index)++;
    }

    if(button(ctx,
              v4(0, (ctx->input->window_dim.h - button_h)/2, button_w, button_h),
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
    V4 body = v4(pad.left, pad.top, ctx->input->window_dim.w - pad.right, ctx->input->window_dim.h - pad.bottom);

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
    V4 screen = v4(0, 0, ctx->input->window_dim.w, ctx->input->window_dim.h);

    Theme default_theme = get_default_theme(ctx->screen);
    V4 bg_color = default_theme.bg_color;
    if(ctx->is_dark) { bg_color = default_theme.primary_color; }
    //push_rect_render_command(ctx->cmd_buffer, screen, bg_color);
}

void
ui_draw_fps_counter(Imui_Context *ctx)
{
    Zhc_Input *input = ctx->input;
    Theme default_theme = get_default_theme(ctx->screen);
    int32 old_font_size = ctx->system_font.size;

    recalculate_font_metrics(ctx->assets, &ctx->system_font, em(default_theme, 0.4f));
    V4 fps_body = v4(10, 10, 1000, 1000);
    char fps_text[16];
    sprintf(fps_text, "%.02f fps", (1000.0f / input->last_frame_in_ms));
    ui_textarea(ctx, &ctx->system_font, fps_body, default_theme.primary_color, fps_text, array_count(fps_text));
    recalculate_font_metrics(ctx->assets, &ctx->system_font, old_font_size);
}

Imui_Context *
ui_context_init(DGL_Mem_Arena *permanent_arena, DGL_Mem_Arena *transient_arena, Render_Command_Buffer *cmd_buffer)
{
    LOG_DEBUG("Permanent arena %p, Transient arena %p", permanent_arena, transient_arena);
    Imui_Context *result = dgl_mem_arena_push_struct(permanent_arena, Imui_Context);
    result->id_stack.count = 64; /* NOTE(dgl): Max count of elements. Increase if necessary */
    result->id_stack.memory = dgl_mem_arena_push_array(permanent_arena, Element_ID, result->id_stack.count);
    result->element_state_list.count = 64; /* NOTE(dgl): Max count of elements. Increase if necessary */
    result->element_state_list.memory = dgl_mem_arena_push_array(permanent_arena, Element_State, result->element_state_list.count);
    result->cmd_buffer = cmd_buffer;

    result->assets = assets_begin_allocate(permanent_arena, ZHC_ASSET_MEMORY_SIZE);
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
                if(strcmp(icon_set->filename, "24x24.png") == 0)
                {
                    result->icon_sets[index++] = initialize_icons(result->assets, icon_set, 24);
                }
                else if(strcmp(icon_set->filename, "32x32.png") == 0)
                {
                    result->icon_sets[index++] = initialize_icons(result->assets, icon_set, 32);
                }
                else if(strcmp(icon_set->filename, "48x48.png") == 0)
                {
                    result->icon_sets[index++] = initialize_icons(result->assets, icon_set, 48);
                }
                else if(strcmp(icon_set->filename, "64x64.png") == 0)
                {
                    result->icon_sets[index++] = initialize_icons(result->assets, icon_set, 64);
                }
                else if(strcmp(icon_set->filename, "96x96.png") == 0)
                {
                    result->icon_sets[index++] = initialize_icons(result->assets, icon_set, 96);
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

    return(result);
}

void
ui_context_update(Imui_Context *ctx, Zhc_Input *input)
{
    ctx->input = input;

    // NOTE(dgl): if the hot element id did not get updated on the last frame, we reset the id.
    if(!ctx->hot_updated) { ctx->hot = 0; }
    ctx->hot_updated = false;

    // NOTE(dgl): update theme if the screen size has changed from desktop to e.g. mobile
    if(maybe_update_screen_size(ctx))
    {
        Theme default_theme = get_default_theme(ctx->screen);
        recalculate_font_metrics(ctx->assets, &ctx->system_font, em(default_theme, 1));

        // NOTE(dgl): only resize the text font if it is not initialized.
        // TODO(dgl): initialize this font size from config
        if(ctx->text_font.size == 0)
        {
            recalculate_font_metrics(ctx->assets, &ctx->text_font, em(default_theme, 1));
        }

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
