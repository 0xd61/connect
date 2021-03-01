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

global Zhc_Platform_Api platform;

#include "zhc_renderer.cpp"
// TODO(dgl): should we move the rendering calls out of UI and
// use a render command buffer?
#include "zhc_ui.cpp"

#define ZHC_VERSION "0.1.0"

enum Net_Msg_Header_Type
{
    Net_Msg_Header_Noop,
    Net_Msg_Header_Hash_Req,
    Net_Msg_Header_Hash_Res,
    Net_Msg_Header_Data_Req,
    Net_Msg_Header_Data_Res
};

struct Net_Msg_Header
{
    uint32 version; /* 16 bit major, 8 bit minor, 8 bit patch */
    Net_Msg_Header_Type type;
    uint32 size;
};

struct File
{
    Zhc_File_Info *info;
    uint8 *data;
};

struct Lib_State
{
    DGL_Mem_Arena permanent_arena;
    DGL_Mem_Arena transient_arena; // NOTE(dgl): cleared on each frame

    Imui_Context *ui_ctx;

    DGL_Mem_Arena io_arena; // NOTE(dgl): cleared on each update timeout
    real32 io_update_timeout;
    File active_file;
    Zhc_File_Group *files;
    int32 desired_file_id;

    Zhc_Net_Socket net_socket;

    bool32 is_initialized;
};

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

internal uint32
parse_version(char *string)
{
    uint32 result = 0;
    int32 segment = 0;
    int32 number = 0;
    while(*string)
    {
        if(*string == '.')
        {
            // NOTE(dgl): major
            if(segment == 0)
            {
                assert(number <= 0xFFFF && number >= 0, "Invalid version segment (cannot be bigger than 65535)");
                result |= (cast(uint32)number << 16);
            }
            // NOTE(dgl): minor and patch
            else
            {
                assert(number <= 0xFF && number >= 0, "Invalid version segment (cannot be bigger than 255)");
                result |= (cast(uint32)number << (segment * 8));
            }

            ++segment;
            number = 0;
            ++string;
        }

        number *= 10;
        number += *string - '0';

        ++string;
    }
    assert(segment == 2, "Failed parsing the version number (too many segments)");
    return(result);
}

internal Zhc_Net_Socket
net_init_socket(DGL_Mem_Arena *arena, char *ip, uint16 port)
{
    Zhc_Net_Socket result = {};

    int32 index = 0;
    int32 number = 0;
    while(*ip)
    {
        if(*ip == '.')
        {
            assert(number <= 0xFF && number >= 0, "Invalid IP segment (cannot be bigger than 255)");
            result.address.ip[index++] = cast(uint8)number;
            number = 0;
            ++ip;
        }

        number *= 10;
        number += *ip - '0';

        ++ip;
    }

    assert(number <= 0xFF && number >= 0, "Invalid IP segment (cannot be bigger than 255)");
    assert(index == 3, "Invalid ip address");
    result.address.ip[index] = cast(uint8)number;
    result.address.port = port;

    platform.setup_socket(arena, &result);

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
    ren_draw_rectangle(ui_ctx->buffer, screen, ui_ctx->bg_color);

    // NOTE(dgl): draw file, if it is available
    if(state->active_file.data)
    {
        ui_main_text(ui_ctx, (char *)state->active_file.data, state->active_file.info->size);
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
            state->desired_file_id = dgl_clamp(state->desired_file_id + 1, 0, state->files->count - 1);
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
        LOG_DEBUG("Resizing font from %f to %f", ui_ctx->text_font->size, ui_ctx->desired_text_font_size);
        dgl_mem_arena_free_all(&ui_ctx->dyn_font_arena);
        ui_ctx->text_font = initialize_font(&ui_ctx->dyn_font_arena, ui_ctx->text_font->ttf_buffer, ui_ctx->desired_text_font_size);
    }

    // NOTE(dgl): update active file if requested
    Zhc_File_Info *info = get_file_info(state->files, state->desired_file_id);
    if(info && info != state->active_file.info)
    {
        state->active_file = read_active_file(&state->io_arena, state->files, info);
    }

    // NOTE(dgl): Check if data is available on the socket.
    Net_Msg_Header header = {};
    bool32 data_available = true;
    while(data_available)
    {
        Zhc_Net_IP address = {};
        data_available = platform.receive_data(&state->net_socket, &address, &header, sizeof(header));

        if(header.type > 0)
        {
            assert(address.host > 0 && address.port > 0, "Platform layer must fill the address struct");

            // TODO(dgl): handle requests with the command buffer
            switch(header.type)
            {
                case Net_Msg_Header_Hash_Req:
                {
                    LOG_DEBUG("File hash handling currently not implemented");
                } break;
                case Net_Msg_Header_Data_Req:
                {
                    if(state->active_file.data)
                    {
                        header.type = Net_Msg_Header_Data_Res;
                        header.version = parse_version(ZHC_VERSION);
                        header.size = dgl_safe_size_to_uint32(state->active_file.info->size);
                        platform.send_data(&state->net_socket, &address, &header, sizeof(header));

                        uint32 filehash = HASH_OFFSET_BASIS;
                        hash(&filehash, state->active_file.data, header.size);
                        LOG_DEBUG("Filehash %u, 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x",
                                  filehash, state->active_file.data[0],
                                  state->active_file.data[1],
                                  state->active_file.data[2],
                                  state->active_file.data[3],
                                  state->active_file.data[4],
                                  state->active_file.data[5],
                                  state->active_file.data[6],
                                  state->active_file.data[7]);

                        platform.send_data(&state->net_socket, &address, state->active_file.data, header.size);
                    }
                    else
                    {
                        header.type = Net_Msg_Header_Data_Res;
                        header.version = parse_version(ZHC_VERSION);
                        header.size = 0;
                        platform.send_data(&state->net_socket, &address, &header, sizeof(header));
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
        // TODO(dgl): create transient storage and put the initializing stuff there instead of a temp arena.

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
    ren_draw_rectangle(ui_ctx->buffer, screen, ui_ctx->bg_color);

    // NOTE(dgl): draw file, if it is available
    if(state->active_file.data)
    {
        ui_main_text(ui_ctx, (char *)state->active_file.data, state->active_file.info->size);
    }

    ui_menu(ui_ctx,
            rect(ui_ctx->window.w - 300, 0, 300, 100),
            color(ui_ctx->fg_color.r, ui_ctx->fg_color.g, ui_ctx->fg_color.b, 0.025f),
            color(ui_ctx->fg_color.r, ui_ctx->fg_color.g, ui_ctx->fg_color.b, 0.5f));

    // NOTE(dgl): update font size, if requested
    if(ui_ctx->desired_text_font_size != ui_ctx->text_font->size)
    {
        LOG_DEBUG("Resizing font from %f to %f", ui_ctx->text_font->size, ui_ctx->desired_text_font_size);
        dgl_mem_arena_free_all(&ui_ctx->dyn_font_arena);
        ui_ctx->text_font = initialize_font(&ui_ctx->dyn_font_arena, ui_ctx->text_font->ttf_buffer, ui_ctx->desired_text_font_size);
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
            Net_Msg_Header header = {};
            // TODO(dgl): check for file hash first and then ask for the full data
            // if the file hash does not match with the existing data.
            header.type = Net_Msg_Header_Data_Req;
            header.version = parse_version(ZHC_VERSION);
            header.size = 0;

            platform.send_data(&state->net_socket, &state->net_socket.address, &header, sizeof(header));

            // NOTE(dgl): On the client side we don't care about the address. There is currently only
            // one server. This is just to fullfil the api specification. Maybe we will need it later,
            // or we will fill it in the platform layer to have a more complete implementation. But for
            // now this is not implemented on the client platform layer.
            Zhc_Net_IP _address = {};

            // NOTE(dgl): The receive_call is blocking. This is why we only check for incoming messages
            // after we sent the data. In the future we will switch to non blocking calls @@cleanup
            platform.receive_data(&state->net_socket, &_address, &header, sizeof(header));

            if(header.type > 0)
            {
                // TODO(dgl): handle requests with the command buffer
                switch(header.type)
                {
                    case Net_Msg_Header_Hash_Res:
                    {
                        LOG_DEBUG("File hash handling currently not implemented");
                    } break;
                    case Net_Msg_Header_Data_Res:
                    {
                        if(header.size > 0)
                        {
                            // NOTE(dgl): cleanup IO arena
                            state->active_file.info = 0;
                            state->active_file.data = 0;
                            dgl_mem_arena_free_all(&state->io_arena);

                            // NOTE(dgl): push new data into the arena
                            Zhc_File_Info *info = dgl_mem_arena_push_struct(&state->io_arena, Zhc_File_Info);
                            info->filename = "\0";
                            info->size = header.size;
                            uint8 *data = dgl_mem_arena_push_array(&state->io_arena, uint8, info->size);
                            platform.receive_data(&state->net_socket, &_address, data, info->size);

                            state->active_file.info = info;
                            state->active_file.data = data;

                            uint32 filehash = HASH_OFFSET_BASIS;
                            hash(&filehash, state->active_file.data, state->active_file.info->size);

                            LOG_DEBUG("Filehash %u, 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x",
                                  filehash, state->active_file.data[0],
                                  state->active_file.data[1],
                                  state->active_file.data[2],
                                  state->active_file.data[3],
                                  state->active_file.data[4],
                                  state->active_file.data[5],
                                  state->active_file.data[6],
                                  state->active_file.data[7]);
                        }
                    } break;
                    default:
                    {
                        LOG_DEBUG("Unhandled network message with type %d (version %u)", header.type, header.version);
                    }
                }
            }
        }
    }
    else
    {
        // TODO(dgl): close socket if there is any
        state->net_socket = net_init_socket(&state->permanent_arena, "127.0.0.1", 1337);
    }

    // NOTE(dgl): put this at the end of the frame
    // to know which element is hot if they are overlapping
    // on the next frame
    ui_ctx->top_most_hot = ui_ctx->hot;
}
