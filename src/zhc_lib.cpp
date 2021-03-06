/*
    NOTE(dgl): TODOs
    - dynamic folder (where we search for the files) + folder dialog
    - config file
    - Some kind of overflow in stbtt_BakeFontBitmap for (108px size fonts)
*/

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

#include "zhc_lib.h"
#include "zhc_net.cpp"
#include "zhc_renderer.cpp"
// TODO(dgl): should we move the rendering calls out of UI and
// use a render command buffer?
#include "zhc_ui.cpp"

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
        if(info->size < ZHC_MAX_FILESIZE)
        {
            result.info = info;
            result.data = dgl_mem_arena_push_array(arena, uint8, result.info->size);

            DGL_Mem_Temp_Arena temp = dgl_mem_arena_begin_temp(arena);
            {
                DGL_String_Builder temp_builder = dgl_string_builder_init(temp.arena, 128);
                dgl_string_append(&temp_builder, "%s%s", group->dirpath, result.info->filename);

                char *filepath = dgl_string_c_style(&temp_builder);
                LOG_DEBUG("Loading file %s", filepath);
                platform.read_entire_file(filepath, result.data, result.info->size);
            }
            dgl_mem_arena_end_temp(temp);
            result.hash = HASH_OFFSET_BASIS;
            hash(&result.hash, result.data, result.info->size);
        }
        else
        {
            // TODO(dgl): create popup error window.
            LOG("Could not load file %s. Filesize (%zu) is bigger than ZHC_MAX_FILESIZE (%zu).", info->filename, info->size, ZHC_MAX_FILESIZE);
        }
    }
    return(result);
}

void
zhc_update_and_render_server(Zhc_Memory *memory, Zhc_Input *input, Zhc_Offscreen_Buffer *buffer)
{
    assert(sizeof(Lib_State) < memory->permanent_storage_size, "Not enough memory allocated");
    platform = memory->api;

    Lib_State *state = cast(Lib_State *)memory->permanent_storage;
    if(!state->is_initialized)
    {
        LOG_DEBUG("Lib_State size: %lld, Available memory: %lld", sizeof(*state), memory->permanent_storage_size);
        dgl_mem_arena_init(&state->permanent_arena, (uint8 *)memory->permanent_storage + sizeof(*state), ((DGL_Mem_Index)memory->permanent_storage_size - sizeof(*state)));
        dgl_mem_arena_init(&state->transient_arena, (uint8 *)memory->transient_storage, (DGL_Mem_Index)memory->transient_storage_size);

        state->net_socket = net_init_socket(&state->permanent_arena, "0.0.0.0", 1337);

        state->ui_ctx = ui_context_init(&state->permanent_arena);

        // NOTE(dgl): Initialize IO Context
        // The io_arena must be initialized before beginnging the temp arena. Otherwise the allocation
        // is automatically freed on the end of the temp arena.
        usize io_arena_size = megabytes(8);
        uint8 *io_arena_base = dgl_mem_arena_push_array(&state->permanent_arena, uint8, io_arena_size);
        dgl_mem_arena_init(&state->io_arena, io_arena_base, io_arena_size);
        state->io_update_timeout = 0.0f;

        // TODO(dgl): let user set this folder and store in config
        DGL_String_Builder temp_builder = dgl_string_builder_init(&state->transient_arena, 128);

        if(platform.get_user_data_base_path(&temp_builder))
        {
            char *temp_target = dgl_string_c_style(&temp_builder);
            Zhc_File_Group *group = platform.get_directory_filenames(&state->io_arena, temp_target);

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

        // NOTE(dgl): clear the input for the first frame because
        // sometimes the return action was triggert from executing
        // the application
        zhc_input_reset(input);
        state->is_initialized = true;
    }

    // NOTE(dgl): @@temporary only until we have a command buffer
    Imui_Context *ui_ctx = state->ui_ctx;
    ui_context_update(state->ui_ctx, input, buffer);

    // NOTE(dgl): draw backplate
    V4 screen = rect(0, 0, ui_ctx->window.w, ui_ctx->window.h);
    ren_draw_rectangle(ui_ctx->buffer, screen, ui_ctx->theme.bg_color);

    // NOTE(dgl): draw file, if it is available
    if(state->active_file.data)
    {
        ui_main_text(ui_ctx, (char *)state->active_file.data, state->active_file.info->size);
    }

    ui_menu(ui_ctx,
            rect(ui_ctx->window.w - 300, 0, 300, 100),
            color(ui_ctx->theme.bg_color.r, ui_ctx->theme.bg_color.g, ui_ctx->theme.bg_color.b, 0.2f),
            color(ui_ctx->theme.fg_color.r, ui_ctx->theme.fg_color.g, ui_ctx->theme.fg_color.b, 0.2f));

    // TODO(dgl): use command buffer instead of desired file etc..
    int32 button_w = 100;
    int32 button_h = 400;
    if(ui_button(ui_ctx,
                 rect(ui_ctx->window.w - button_w, (ui_ctx->window.h - button_h)/2, button_w, button_h),
                 color(ui_ctx->theme.bg_color.r, ui_ctx->theme.bg_color.g, ui_ctx->theme.bg_color.b, 0.2f),
                 color(ui_ctx->theme.fg_color.r, ui_ctx->theme.fg_color.g, ui_ctx->theme.fg_color.b, 0.2f),
                 ui_ctx->system_font, ">") ||
       input_pressed(ui_ctx->input, Zhc_Keyboard_Button_Right) ||
       input_pressed(ui_ctx->input, Zhc_Keyboard_Button_Enter) ||
       input_pressed(ui_ctx->input, ' '))
    {
        if(state->files)
        {
            state->desired_file_id = dgl_clamp(state->desired_file_id + 1, 0, state->files->count - 1);
        }
    }

    if(ui_button(ui_ctx,
                 rect(0, (ui_ctx->window.h - button_h)/2, button_w, button_h),
                 color(ui_ctx->theme.bg_color.r, ui_ctx->theme.bg_color.g, ui_ctx->theme.bg_color.b, 0.2f),
                 color(ui_ctx->theme.fg_color.r, ui_ctx->theme.fg_color.g, ui_ctx->theme.fg_color.b, 0.2f),
                 ui_ctx->system_font, "<") ||
       input_pressed(ui_ctx->input, Zhc_Keyboard_Button_Left))
    {
        if(state->files)
        {
            state->desired_file_id = dgl_clamp(state->desired_file_id - 1, 0, state->files->count - 1);
        }
    }

    // NOTE(dgl): Reload the directory/file info and file every 10 seconds.
    state->io_update_timeout += input->last_frame_in_ms;
    if(state->io_update_timeout > 10000.0f && state->files->count > 0)
    {
        // NOTE(dgl): copy current infos to have them available after
        // puring the io arena. // TODO(dgl): Is there a better way?
        DGL_String_Builder tmp_builder = dgl_string_builder_init(&state->transient_arena, 128);
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
            Zhc_File_Info *info = get_file_info(state->files, state->desired_file_id);
            // NOTE(dgl): we resetted the file earlier. Therefore we do not need to reset if
            // the file does not exist anymore.
            if(info)
            {
                state->active_file = read_active_file(&state->io_arena, state->files, info);
            }
        }
    }

    // NOTE(dgl): update font size, if requested
    if(ui_ctx->desired_text_font_size != ui_ctx->text_font->size)
    {
        LOG_DEBUG("Resizing font from %d to %d", ui_ctx->text_font->size, ui_ctx->desired_text_font_size);
        ui_resize_font(&ui_ctx->font_arena, ui_ctx->text_font, ui_ctx->desired_text_font_size);
    }

    // NOTE(dgl): update active file if requested
    Zhc_File_Info *info = get_file_info(state->files, state->desired_file_id);
    if(info && info != state->active_file.info)
    {
        state->active_file = read_active_file(&state->io_arena, state->files, info);
    }

    // NOTE(dgl): Check if data is available on the socket.
    bool32 data_available = true;
    while(data_available)
    {
        Zhc_Net_IP address = {};
        Net_Msg_Header header = {};

        data_available = net_recv_header(&state->net_socket, &address, &header);

        if(header.type > 0)
        {
            assert(address.host > 0 && address.port > 0, "Platform layer must fill the address struct");

            // TODO(dgl): proper message sending @@cleanup
            switch(header.type)
            {
                case Net_Msg_Header_Hash_Req:
                {
                    if(state->active_file.hash)
                    {
                        net_send_hash_response(&state->net_socket, &address, state->active_file.hash);
                    }
                    else
                    {
                        net_send_header(&state->net_socket, &address, Net_Msg_Header_Hash_Res);
                    }
                } break;
                case Net_Msg_Header_Data_Req:
                {
                    if(state->active_file.data)
                    {
                        net_send_data_response(&state->net_socket, &address, state->active_file.data, state->active_file.info->size);
                    }
                    else
                    {
                        net_send_header(&state->net_socket, &address, Net_Msg_Header_Data_Res);
                    }
                } break;
                default:
                {
                    LOG_DEBUG("Unhandled network message with type %d (version %u)", header.type, header.version);
                }
            }
        }
    }

    // NOTE(dgl): put this at the end of the frame
    // to know which element is hot if they are overlapping
    // on the next frame
    ui_ctx->top_most_hot = ui_ctx->hot;
}

void
zhc_update_and_render_client(Zhc_Memory *memory, Zhc_Input *input, Zhc_Offscreen_Buffer *buffer)
{
    assert(sizeof(Lib_State) < memory->permanent_storage_size, "Not enough memory allocated");
    platform = memory->api;

    Lib_State *state = cast(Lib_State *)memory->permanent_storage;
    if(!state->is_initialized)
    {
        LOG_DEBUG("Lib_State size: %lld, Available memory: %lld", sizeof(*state), memory->permanent_storage_size);
        dgl_mem_arena_init(&state->permanent_arena, (uint8 *)memory->permanent_storage + sizeof(*state), ((DGL_Mem_Index)memory->permanent_storage_size - sizeof(*state)));
        dgl_mem_arena_init(&state->transient_arena, (uint8 *)memory->transient_storage, (DGL_Mem_Index)memory->transient_storage_size);

        state->ui_ctx = ui_context_init(&state->permanent_arena);

        // NOTE(dgl): Initialize IO Context
        // The io_arena must be initialized before beginnging the temp arena. Otherwise the allocation
        // is automatically freed on the end of the temp arena.
        usize io_arena_size = megabytes(8);
        uint8 *io_arena_base = dgl_mem_arena_push_array(&state->permanent_arena, uint8, io_arena_size);
        dgl_mem_arena_init(&state->io_arena, io_arena_base, io_arena_size);
        state->io_update_timeout = 0.0f;

        // NOTE(dgl): clear the input for the first frame because
        // sometimes the return action was triggert from executing
        // the application
        zhc_input_reset(input);
        state->is_initialized = true;
    }

    Imui_Context *ui_ctx = state->ui_ctx;
    ui_context_update(state->ui_ctx, input, buffer);

    // NOTE(dgl): draw backplate
    V4 screen = rect(0, 0, ui_ctx->window.w, ui_ctx->window.h);
    ren_draw_rectangle(ui_ctx->buffer, screen, ui_ctx->theme.bg_color);

    // NOTE(dgl): draw file, if it is available
    if(state->active_file.data)
    {
        ui_main_text(ui_ctx, (char *)state->active_file.data, state->active_file.info->size);
    }

    ui_menu(ui_ctx,
            rect(ui_ctx->window.w - 300, 0, 300, 100),
            color(ui_ctx->theme.fg_color.r, ui_ctx->theme.fg_color.g, ui_ctx->theme.fg_color.b, 0.025f),
            color(ui_ctx->theme.fg_color.r, ui_ctx->theme.fg_color.g, ui_ctx->theme.fg_color.b, 0.5f));

    // NOTE(dgl): update font size, if requested
    if(ui_ctx->desired_text_font_size != ui_ctx->text_font->size)
    {
        LOG_DEBUG("Resizing font from %f to %f", ui_ctx->text_font->size, ui_ctx->desired_text_font_size);
        ui_resize_font(&ui_ctx->font_arena, ui_ctx->text_font, ui_ctx->desired_text_font_size);
    }

    // TODO(dgl): blocks until timeout is hit or connection is
    // established. Need to draw something and then try to connect.
    // If this is too much of a hassle, we will use a background thread.
    if(state->net_socket.no_error)
    {
        state->io_update_timeout += input->last_frame_in_ms;
        if(state->io_update_timeout > 1000.0f)
        {
            state->io_update_timeout = 0;
            net_send_hash_request(&state->net_socket, &state->net_socket.address);
        }


        // NOTE(dgl): On the client side we don't care about the address. There is currently only
        // one server. This is just to fullfil the api specification. Maybe we will need it later,
        // or we will fill it in the platform layer to have a more complete implementation. But for
        // now this is not implemented on the client platform layer.
        Net_Msg_Header header = {};
        Zhc_Net_IP _address = {};

        net_recv_header(&state->net_socket, &_address, &header);
        if(header.type > 0)
        {
            // TODO(dgl): handle requests with the command buffer
            switch(header.type)
            {
                case Net_Msg_Header_Hash_Res:
                {
                    if(header.size > 0)
                    {
                        uint32 hash = 0;
                        net_recv_data(&state->net_socket, &_address, &hash, sizeof(hash));

                        if(hash != state->active_file.hash)
                        {
                            LOG_DEBUG("Hash mismatch - remote: %u, local: %u", hash, state->active_file.hash);
                            net_send_data_request(&state->net_socket, &state->net_socket.address);
                        }
                    }
                } break;
                case Net_Msg_Header_Data_Res:
                {
                    if(header.size > 0)
                    {
                        // NOTE(dgl): cleanup IO arena
                        state->active_file = {};
                        dgl_mem_arena_free_all(&state->io_arena);

                        // NOTE(dgl): push new data into the arena
                        Zhc_File_Info *info = dgl_mem_arena_push_struct(&state->io_arena, Zhc_File_Info);
                        info->filename = "\0";
                        info->size = header.size;
                        uint8 *data = dgl_mem_arena_push_array(&state->io_arena, uint8, info->size);
                        net_recv_data(&state->net_socket, &_address, data, info->size);

                        state->active_file.info = info;
                        state->active_file.data = data;
                        state->active_file.hash = HASH_OFFSET_BASIS;
                        hash(&state->active_file.hash, state->active_file.data, state->active_file.info->size);
                    }
                } break;
                default:
                {
                    LOG_DEBUG("Unhandled network message with type %d (version %u)", header.type, header.version);
                }
            }
        }
    }
    else
    {
        // TODO(dgl): close socket if there is any error
        //state->net_socket = net_init_socket(&state->permanent_arena, "192.168.101.124", 1337);
        state->net_socket = net_init_socket(&state->permanent_arena, "127.0.0.1", 1337);
    }

    // NOTE(dgl): put this at the end of the frame
    // to know which element is hot if they are overlapping
    // on the next frame
    ui_ctx->top_most_hot = ui_ctx->hot;
}
