/*
    NOTE(dgl): TODOs
    - dynamic folder (where we search for the files) + folder dialog
    - config file
    - Some kind of overflow in stbtt_BakeFontBitmap for (108px size fonts)
    - Render circles
    - Cached rendering
    - Rerender when switching to landscape (should work now)
    - Fast light tapping is not recognized (how can I debug this? Clicking in th emulator works totally fine -> Will be easier when we switch to render commands)
    - Better packet buffer strategy to be able to resend if necessary
    - Packet throttle for large chunks to not flood the bandwidth
    - Image resizing for better size control of assets


    - With the new render command feature, assets are freed/changed before we have rendered them (font bitmap -> fontsize change.)
    - I guess we have to store each font size separately?
    - We could also push the desired fontsize into the command and bake the bitmap during render.
    - However we need a baked bitmap during ui creation to get the glyph sizes.
*/

#include "zhc_lib.h"
#include "zhc_asset.cpp"
#include "zhc_renderer.cpp"
#include "zhc_ui.cpp"
#include "zhc_crypto.cpp"
#include "zhc_net.cpp"

#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#define STB_IMAGE_IMPLEMENTATION

#define STBTT_STATIC
#define STBI_STATIC

#define STBI_ASSERT(x) assert(x, "stb assert")
#define STBTT_assert(x) assert(x, "stb assert")

// NOTE(dgl): Disable compiler warnings for stb includes
#if defined(__clang__)
#pragma clang diagnostic push
#if __clang_major__ > 7
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#endif
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wfloat-conversion"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"

#include "lib/stb_truetype.h"

#define STBI_NO_JPEG
#define STBI_NO_BMP
#define STBI_NO_TGA
#define STBI_NO_PSD
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_GIF
#define STBI_NO_PNM
#include "lib/stb_image.h"

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

#include "lib/stb_truetype.h"
#include "lib/stb_image.h"

#pragma warning(pop)
#endif

internal void
close_files(Zhc_File_Group *files)
{
    Zhc_File_Info *info = files->first_file_info;
    files->first_file_info = 0;
    files->count = 0;
    if(info)
    {
        platform.close_file(info);
        info = info->next;
    }
}

internal void
multicast_request(Net_Context *ctx, Net_Message_Type type)
{
    Net_Message message = {};
    message.type = type;
    net_multicast_message(ctx, message);
}

internal bool32
file_is_valid(File *file)
{
    bool32 result = false;
    // TODO(dgl): proper file is valid check
    // type, size, has info, has data
    if(file->data)
    {
        result = true;
    }
    return(result);
}

internal void
send_filehash(Net_Context *ctx, Net_Conn_ID id, File *file)
{
    if(file_is_valid(file))
    {
        Net_Message message = {};
        message.type = Net_Message_Hash_Res;
        message.payload = cast(uint8 *)&file->hash;
        message.payload_size = sizeof(file->hash);
        LOG_DEBUG("Sending hash %u", file->hash);
        net_send_message(ctx, id, message);
    }
}

internal void
send_file(Net_Context *ctx, Net_Conn_ID id, File *file)
{
    if(file_is_valid(file))
    {
        Net_Message message = {};
        message.type = Net_Message_Data_Res;
        message.payload = file->data;
        message.payload_size = file->info->size;
        LOG_DEBUG("Sending %d bytes of data with hash %u", message.payload_size, file->hash);
        net_send_message(ctx, id, message);
    }
}

internal void
multicast_file(Net_Context *ctx, File *file)
{
    LOG_DEBUG("Multicast file %p", file->data);
    if(file_is_valid(file))
    {
        Net_Message message = {};
        message.type = Net_Message_Data_Res;
        message.payload = file->data;
        message.payload_size = file->info->size;
        LOG_DEBUG("Multicasting %d bytes of data with hash %u", message.payload_size, file->hash);
        net_multicast_message(ctx, message);
    }
}

internal bool32
input_updated(Zhc_Input *a, Zhc_Input *b)
{
    bool32 result = false;
    result = ((a->window_dim.w != b->window_dim.w) ||
              (a->window_dim.h != b->window_dim.h) ||
              (a->pos.x != b->pos.x) ||
              (a->pos.y != b->pos.y) ||
              (a->last_pos.x != b->last_pos.x) ||
              (a->last_pos.y != b->last_pos.y) ||
              (a->scroll_delta.x != b->scroll_delta.x) ||
              (a->scroll_delta.y != b->scroll_delta.y) ||
              (a->key_down != b->key_down) ||
              (a->key_pressed != b->mouse_down) ||
              (a->mouse_down != b->mouse_down) ||
              strcmp(a->text, b->text) != 0);

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

// NOTE(dgl): By having the old file parameter, we check if the file currently loaded
// is the same as the requested one. If they are the same we do not load this file.
internal File
read_file(DGL_Mem_Arena *arena, Zhc_File_Group *group, File *old_file, int file_id)
{
    File result = {};

    if(group)
    {
        assert(old_file, "Current file cannot be NULL");
        // NOTE(dgl): update active file if requested
        Zhc_File_Info *info = get_file_info(group, file_id);
        if(info && info != old_file->info)
        {
            if(info->size < ZHC_MAX_FILESIZE)
            {
                result.info = info;
                result.data = dgl_mem_arena_push_array(arena, uint8, result.info->size);

                LOG_DEBUG("Loading file %s", result.info->filename);
                platform.read_entire_file(&result.info->handle, result.data, result.info->size);
                assert(result.info->handle.no_error, "Failed loading the file");

                result.hash = HASH_OFFSET_BASIS;
                hash(&result.hash, result.data, result.info->size);
            }
            else
            {
                // TODO(dgl): create popup error window.
                LOG("Could not load file %s. Filesize (%zu) is bigger than ZHC_MAX_FILESIZE (%zu).", info->filename, info->size, ZHC_MAX_FILESIZE);
            }
        }
        else
        {
            result = *old_file;
        }
    }
    return(result);
}

bool32
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

        state->net_ctx = net_init_server(&state->permanent_arena);
        state->ui_ctx = ui_context_init(&state->permanent_arena, &state->transient_arena, &state->cmd_buffer);

        // NOTE(dgl): Initialize IO Context
        // The io_arena must be initialized before beginnging the temp arena. Otherwise the allocation
        // is automatically freed on the end of the temp arena.
        usize io_arena_size = ZHC_IO_MEMORY_SIZE;
        uint8 *io_arena_base = dgl_mem_arena_push_array(&state->permanent_arena, uint8, io_arena_size);
        dgl_mem_arena_init(&state->io_arena, io_arena_base, io_arena_size);
        state->io_update_timeout = 0.0f;

        // TODO(dgl): let user set this folder and store in config
        DGL_String_Builder temp_builder = dgl_string_builder_init(&state->transient_arena, 128);

        if(platform.get_user_data_base_path(&temp_builder))
        {
            char *temp_target = dgl_string_c_style(&temp_builder);
            state->files = platform.get_directory_filenames(&state->io_arena, temp_target);
            state->active_file = read_file(&state->io_arena, state->files, &state->active_file, state->desired_file_id);
        }

        // NOTE(dgl): clear the input for the first frame because
        // sometimes the return action was triggert from executing
        // the application
        zhc_input_reset(input);

        int32 cache_cells_x = 20;
        int32 cache_cells_y = 11;
        if(buffer->height > buffer->width)
        {
            int32 tmp = cache_cells_x;
            cache_cells_x = cache_cells_y;
            cache_cells_y = tmp;
        }
        state->render_ctx.grid = renderer_buid_hash_grid(&state->permanent_arena, cache_cells_x, cache_cells_y);

        state->force_render = true;
        state->is_initialized = true;
    }

    bool32 do_render = false;

    if(input->has_window_event)
    {
        do_render = true;
    }
    else if(input_updated(&state->old_input, input))
    {
        do_render = true;
        state->old_input = *input;
        state->force_render = true;
    }
    else
    {
        do_render = state->force_render;
        state->force_render = false;
    }

    dgl_mem_arena_free_all(&state->transient_arena);
    // NOTE(dgl): we reinit the command buffer on each loop.
    usize render_buffer_size = kilobytes(512);
    uint8 *render_buffer_base = dgl_mem_arena_push_array(&state->transient_arena, uint8, render_buffer_size);
    render_command_buffer_init(&state->cmd_buffer, render_buffer_base, render_buffer_size);

    if(state->net_ctx->socket.handle.no_error == false)
    {
        net_open_socket(state->net_ctx);
    }

    // NOTE(dgl): Reload the directory (file infos) every 10 seconds.
    // We clear the whole IO arena for simplicity reasons. This means the
    // active file must be reloaded and will be sent via multicast to
    // all the clients every 10 seconds. @performance
    state->io_update_timeout += input->last_frame_in_ms;
    if(state->io_update_timeout > 10000.0f && state->files->count > 0)
    {
        state->io_update_timeout = 0.0f;

        DGL_String_Builder tmp_builder = dgl_string_builder_init(&state->transient_arena, 128);
        dgl_string_append(&tmp_builder, "%s", state->files->dirpath);
        char *tmp_dir_path = dgl_string_c_style(&tmp_builder);

        close_files(state->files);
        state->active_file = {};
        dgl_mem_arena_free_all(&state->io_arena);

        state->files = platform.get_directory_filenames(&state->io_arena, tmp_dir_path);
    }

    // NOTE(dgl): Update the active file (does nothing if the file is already loaded).
    uint32 old_hash = state->active_file.hash;
    state->active_file = read_file(&state->io_arena, state->files, &state->active_file, state->desired_file_id);
    if(state->active_file.hash != old_hash)
    {
        multicast_file(state->net_ctx, &state->active_file);
        do_render = true;
    }

    Net_Message message = {};
    Net_Conn_ID client = 0;
    while((client = net_recv_message(&state->transient_arena, state->net_ctx, input->last_frame_in_ms, &message)) >= 0)
    {
        switch(message.type)
        {
            case Net_Message_Hash_Req:
            {
                send_filehash(state->net_ctx, client, &state->active_file);
            } break;
            case Net_Message_Data_Req:
            {
                send_file(state->net_ctx, client, &state->active_file);
            } break;
            default:
            {
                // TODO(dgl): do nothing
            }
        }
    }
    net_send_pending_packet_buffers(state->net_ctx);

    if(do_render)
    {
        // NOTE(dgl): @@temporary only until we have a command buffer
        Imui_Context *ui_ctx = state->ui_ctx;
        ui_context_update(ui_ctx, input);

        ui_draw_backplate(ui_ctx);

        if(state->active_file.data)
        {
            ui_draw_textarea(ui_ctx, (char *)state->active_file.data, state->active_file.info->size);
        }

        int32 next_file_id = state->desired_file_id;
        ui_draw_file_controls(ui_ctx, &next_file_id);
        if(state->files)
        {
            state->desired_file_id = dgl_clamp(next_file_id, 0, state->files->count - 1);
        }

        ui_draw_menu(ui_ctx);

#if ZHC_INTERNAL
        ui_draw_fps_counter(ui_ctx);
#endif

        // NOTE(dgl): put this at the end of the frame
        // to know which element is hot if they are overlapping
        // on the next frame
        ui_ctx->top_most_hot = ui_ctx->hot;

        render(&state->render_ctx, ui_ctx->cmd_buffer, ui_ctx->assets, buffer);
    }

    return(do_render);
}

bool32
zhc_update_and_render_client(Zhc_Memory *memory, Zhc_Input *input, Zhc_Offscreen_Buffer *buffer)
{
    assert(sizeof(Lib_State) < memory->permanent_storage_size, "Not enough memory allocated");
    platform = memory->api;

    Lib_State *state = cast(Lib_State *)memory->permanent_storage;
    if(!state->is_initialized)
    {
        dgl_mem_arena_init(&state->permanent_arena, (uint8 *)memory->permanent_storage + sizeof(*state), ((DGL_Mem_Index)memory->permanent_storage_size - sizeof(*state)));
        dgl_mem_arena_init(&state->transient_arena, (uint8 *)memory->transient_storage, (DGL_Mem_Index)memory->transient_storage_size);
        LOG_DEBUG("Permanent memory: %p (%lld), Lib_State size: %lld, permanent_arena: %p, transient_arena: %p", memory->permanent_storage, memory->permanent_storage_size, sizeof(*state), state->permanent_arena.base, state->transient_arena.base);

        state->net_ctx = net_init_client(&state->permanent_arena);

        // NOTE(dgl): at this state the cmd_buffer is not initialized.
        state->ui_ctx = ui_context_init(&state->permanent_arena, &state->transient_arena, &state->cmd_buffer);

        // NOTE(dgl): Initialize IO Context
        // The io_arena must be initialized before beginnging the temp arena. Otherwise the allocation
        // is automatically freed on the end of the temp arena.
        usize io_arena_size = ZHC_IO_MEMORY_SIZE;
        uint8 *io_arena_base = dgl_mem_arena_push_array(&state->permanent_arena, uint8, io_arena_size);
        dgl_mem_arena_init(&state->io_arena, io_arena_base, io_arena_size);
        state->io_update_timeout = 0.0f;

        // NOTE(dgl): clear the input for the first frame because
        // sometimes the return action was triggert from executing
        // the application
        zhc_input_reset(input);

        int32 cache_cells_x = 20;
        int32 cache_cells_y = 11;
        if(buffer->height > buffer->width)
        {
            int32 tmp = cache_cells_x;
            cache_cells_x = cache_cells_y;
            cache_cells_y = tmp;
        }
        state->render_ctx.grid = renderer_buid_hash_grid(&state->permanent_arena, cache_cells_x, cache_cells_y);

        state->force_render = true;
        state->is_initialized = true;
    }

    bool32 do_render = false;

    if(input->has_window_event)
    {
        do_render = true;
    }
    else if(input_updated(&state->old_input, input))
    {
        do_render = true;
        state->old_input = *input;
        state->force_render = true;
    }
    else
    {
        do_render = state->force_render;
        state->force_render = false;
    }

    if(state->net_ctx->socket.handle.no_error == false)
    {
        net_open_socket(state->net_ctx);
    }

    dgl_mem_arena_free_all(&state->transient_arena);
    // NOTE(dgl): we reinit the command buffer on each loop.
    usize render_buffer_size = kilobytes(512);
    uint8 *render_buffer_base = dgl_mem_arena_push_array(&state->transient_arena, uint8, render_buffer_size);
    render_command_buffer_init(&state->cmd_buffer, render_buffer_base, render_buffer_size);

    net_request_server_connection(state->net_ctx);

    Net_Message message = {};
    Net_Conn_ID client = 0;
    while((client = net_recv_message(&state->transient_arena, state->net_ctx, input->last_frame_in_ms, &message)) >= 0)
    {
        switch(message.type)
        {
            case Net_Message_Hash_Res:
            {
                uint32 hash = 0;
                assert(message.payload_size == sizeof(hash), "Invalid hash size");
                hash = *cast(uint32 *)message.payload;
                LOG_DEBUG("Received hash %u, active_file hash %u", hash, state->active_file.hash);
                if(hash != state->active_file.hash)
                {
                    // NOTE(dgl): we can send a multicast here, because the client only has one connection.
                    multicast_request(state->net_ctx, Net_Message_Data_Req);
                }
            } break;
            case Net_Message_Data_Res:
            {
                 state->active_file = {};
                 dgl_mem_arena_free_all(&state->io_arena);

                 // NOTE(dgl): push new data into the arena
                 Zhc_File_Info *info = dgl_mem_arena_push_struct(&state->io_arena, Zhc_File_Info);
                 info->filename = "\0";
                 info->size = message.payload_size;
                 uint8 *data = dgl_mem_arena_push_array(&state->io_arena, uint8, info->size);
                 dgl_memcpy(data, message.payload, info->size);

                 state->active_file.info = info;
                 state->active_file.data = data;
                 state->active_file.hash = HASH_OFFSET_BASIS;
                 hash(&state->active_file.hash, state->active_file.data, state->active_file.info->size);
                 LOG_DEBUG("Received %d bytes of data with the hash %u", info->size, state->active_file.hash);
                 do_render = true;
            } break;
            default:
            {
                // NOTE(dgl): do nothing
            }
        }
    }
    net_send_pending_packet_buffers(state->net_ctx);

    state->io_update_timeout += input->last_frame_in_ms;
    if(state->io_update_timeout > 5000.0f)
    {
        state->io_update_timeout = 0;
        // NOTE(dgl): we can send a multicast here, because the client only has one connection.
        multicast_request(state->net_ctx, Net_Message_Hash_Req);
    }

    if(do_render)
    {
        Imui_Context *ui_ctx = state->ui_ctx;
        ui_context_update(ui_ctx, input);

        ui_draw_backplate(ui_ctx);

        // NOTE(dgl): draw file, if it is available
        if(state->active_file.data)
        {
            ui_draw_textarea(ui_ctx, (char *)state->active_file.data, state->active_file.info->size);
        }

        ui_draw_menu(ui_ctx);

#if ZHC_INTERNAL
        ui_draw_fps_counter(ui_ctx);
#endif

        // NOTE(dgl): put this at the end of the frame
        // to know which element is hot if they are overlapping
        // on the next frame
        ui_ctx->top_most_hot = ui_ctx->hot;

        render(&state->render_ctx, ui_ctx->cmd_buffer, ui_ctx->assets, buffer);
    }

    dgl_mem_arena_free_all(&state->transient_arena);

    return(do_render);
}
