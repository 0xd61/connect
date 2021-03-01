#include "zhc_platform.h"
#include "zhc_lib.cpp"

#ifdef __ANDROID__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#endif

#define DGL_IMPLEMENTATION
#include "dgl.h"

#include <sys/mman.h> /* mmap */
#include <string.h> /* memset, memcpy */
// NOTE(dgl): On Windows this is only included in the MinGW compiler,
// not in Microsoft Visual C++.
#include <dirent.h> /* opendir, readdir */

#define MAX_CLIENTS 4

global bool32 global_running;

struct SDL_Client
{
    IPaddress peer;
    TCPsocket socket;
};

struct SDL_Server_Socket
{
    TCPsocket socket;
    SDLNet_SocketSet set;
    SDL_Client clients[MAX_CLIENTS];
};

internal bool32
is_ip_address(Zhc_Net_IP a, IPaddress b)
{
    bool32 result = false;
    if((a.host == b.host) &&
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
       (a.port == SDL_Swap16(b.port)))
#else
       (a.port == b.port))
#endif
    {
        result = true;
    }

    return(result);
}

internal SDL_Client *
get_client(SDL_Server_Socket *platform, Zhc_Net_IP ip)
{
    SDL_Client *result = 0;
    // NOTE(dgl): ip and port must be greater than 0. There cannot be a
    // client with the IP 0.0.0.0
    if(ip.host > 0 && ip.port > 0)
    {
        for(int32 index = 0; index < array_count(platform->clients); ++index)
        {
            SDL_Client *client = platform->clients + index;
            if(is_ip_address(ip, client->peer))
            {
                result = client;
                break;
            }
        }
    }

    return(result);
}

ZHC_SEND_DATA(sdl_net_send_data)
{
    assert(target_address, "Target address cannot be null");
    SDL_Server_Socket *platform = (SDL_Server_Socket *)socket->platform;
    SDL_Client *client = get_client(platform, *target_address);

    if(client)
    {
        int32 result = SDLNet_TCP_Send(client->socket, buffer, dgl_safe_size_to_int32(buffer_size));
        if(result <= 0)
        {
            LOG("Sending data to socket failed with: %s. Closing socket", SDLNet_GetError());
            SDLNet_TCP_DelSocket(platform->set, client->socket);
            SDLNet_TCP_Close(client->socket);
            client->socket = 0;
            client->peer = {};
        }
        else
        {
            LOG_DEBUG("Sending %d bytes", result);
        }
    }
}

ZHC_RECEIVE_DATA(sdl_net_receive_data)
{
    bool32 result = false;
    SDL_Server_Socket *platform = (SDL_Server_Socket *)socket->platform;

    int32 ready_count = SDLNet_CheckSockets(platform->set, 0);
    if(ready_count > 0)
    {
        // NOTE(dgl): handle new incoming connecitons
        if(SDLNet_SocketReady(platform->socket))
        {
            // TODO(dgl): handle inactive and failed clients (e.g. when the proccess is killed
            // without properly closing the connection)
            TCPsocket client_socket = SDLNet_TCP_Accept(platform->socket);
            if(client_socket)
            {
                --ready_count;
                int32 index = 0;
                while(index < array_count(platform->clients))
                {
                    SDL_Client *client = platform->clients + index++;
                    if(!client->socket)
                    {
                        client->socket = client_socket;
                        client->peer = *SDLNet_TCP_GetPeerAddress(client_socket);
                        SDLNet_TCP_AddSocket(platform->set, client->socket);
                        LOG_DEBUG("New client connection");
                        break;
                    }
                }

                if(index == array_count(platform->clients))
                {
                    LOG("No free client slot available");
                }
            }
        }

        // NOTE(dgl): handle client requests (only if there was activity)
        if(ready_count > 0)
        {
            --ready_count;
            assert(peer_address, "Peer address cannot be null");

            // TODO(dgl): @@cleanup
            for(int32 index = 0; index < array_count(platform->clients); ++index)
            {
                SDL_Client *client = platform->clients + index;

                // NOTE(dgl): if a client address is provided we only check for this specific
                // client, if data is available. We still loop though all clients. @@performance
                // If there is no address and port, we fill the struct with the data of the current
                // client @@cleanup
                if(peer_address->port > 0 && peer_address->host > 0)
                {
                    if(!is_ip_address(*peer_address, client->peer))
                    {
                        continue;
                    }
                }

                if(SDLNet_SocketReady(client->socket))
                {
                    int32 success = SDLNet_TCP_Recv(client->socket, buffer, dgl_safe_size_to_int32(buffer_size));
                    if(success <= 0)
                    {
                        LOG("Receiving data from socket failed with: %s. Closing socket", SDLNet_GetError());
                        SDLNet_TCP_DelSocket(platform->set, client->socket);
                        SDLNet_TCP_Close(client->socket);
                        client->socket = 0;
                        client->peer = {};
                    }
                    else
                    {
                        // NOTE(dgl): if no address was provided we return the peer address
                        // to the caller function
                        if(peer_address->port == 0 && peer_address->host == 0)
                        {
                            peer_address->host = client->peer.host;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
                            peer_address->port = SDL_Swap16(client->peer.port);
#else
                            peer_address->port = client->peer.port;
#endif
                        }

                        LOG_DEBUG("Receiving %d bytes", success);
                    }

                    break;
                }
            }
        }
    }
    else if(ready_count < 0)
    {
        LOG("Checking ready sockets failed with: %s", SDLNet_GetError());
    }

    if(ready_count > 0)
    {
        result = true;
    }

    return(result);
}

ZHC_SETUP_SOCKET(sdl_net_setup_socket_set)
{
    IPaddress ip = {};
    ip.host = socket->address.host;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    ip.port = SDL_Swap16(socket->address.port);
#else
    ip.port = socket->address.port;
#endif

    // NOTE(dgl): 1 server socket and max_clients client sockets
    SDLNet_SocketSet set = SDLNet_AllocSocketSet(MAX_CLIENTS + 1);
    if(set)
    {
        TCPsocket server_tcpsock = SDLNet_TCP_Open(&ip);
        if(server_tcpsock)
        {
            SDLNet_TCP_AddSocket(set, server_tcpsock);

            SDL_Server_Socket *server_socket = dgl_mem_arena_push_struct(arena, SDL_Server_Socket);
            server_socket->socket = server_tcpsock;
            server_socket->set = set;

            socket->platform = server_socket;
            socket->no_error = true;
        }
        else
        {
            LOG("Opening TCP socket failed with: %s", SDLNet_GetError());
        }
    }
    else
    {
        LOG("Allocating sockets failed with: %s", SDLNet_GetError());
    }
}

ZHC_FILE_SIZE(sdl_file_size)
{
    usize result = 0;
    SDL_RWops *io = SDL_RWFromFile(filename, "rb");
    if(io != 0)
    {
        int64 size = SDL_RWsize(io);
        assert(size > 0, "Failed to find file");
        result = cast(usize)size;
        SDL_RWclose(io);
    }
    else
    {
        LOG("SDL_RWFromFile failed for %s with error: %s", filename, SDL_GetError());
    }
    return(result);
}

internal Zhc_File_Info *
allocate_file_info(Zhc_File_Group *group, char *filename)
{
    Zhc_File_Info *result = dgl_mem_arena_push_struct(group->arena, Zhc_File_Info);
    usize filename_size = dgl_string_length(filename);


    // NOTE(dgl): the linked list is sorted by filename.
    Zhc_File_Info *node = group->first_file_info;
    Zhc_File_Info *next_node = 0;
    while(node)
    {
        next_node = node->next;

        if(strcmp(filename, node->filename) > 0)
        {
            if(!next_node || strcmp(filename, next_node->filename) < 0)
            {
                break;
            }
        }
        node = next_node;
        if(next_node)
        {
            next_node = next_node->next;
        }
    }

    if(!node)
    {
        result->next = group->first_file_info;
        group->first_file_info = result;
    }
    else
    {
        node->next = result;
        result->next = next_node;
    }

    result->filename = dgl_mem_arena_push_array(group->arena, char, filename_size + 1);
    dgl_memcpy(result->filename, filename, filename_size);
    result->filename[filename_size] = '\0';
    group->count++;

    return(result);
}

ZHC_GET_DIRECTORY_FILENAMES(get_directory_filenames)
{
    Zhc_File_Group *result = 0;
    DIR *dir = opendir(path);
    if(dir)
    {
        result = dgl_mem_arena_push_struct(arena, Zhc_File_Group);
        result->arena = arena;
        usize dirpath_count = dgl_string_length(path);
        result->dirpath = dgl_mem_arena_push_array(arena, char, dirpath_count + 1);
        dgl_memcpy(result->dirpath, path, dirpath_count);
        result->dirpath[dirpath_count] = '\0';

        struct dirent *entry;
        while((entry = readdir(dir)))
        {
            if(entry->d_name[0] == '.' &&
               entry->d_name[1] == '\0'){ continue; }

            if(entry->d_name[0] == '.' &&
               entry->d_name[1] == '.' &&
               entry->d_name[2] == '\0'){ continue; }

            Zhc_File_Info *info = allocate_file_info(result, entry->d_name);

            DGL_Mem_Temp_Arena temp = dgl_mem_arena_begin_temp(result->arena);

            DGL_String_Builder tmp_builder = dgl_string_builder_init(temp.arena, 128);
#ifdef _WIN32
            dgl_string_append(&tmp_builder, "%s\\%s", result->dirpath, info->filename);
#else
            dgl_string_append(&tmp_builder, "%s/%s", result->dirpath, info->filename);
#endif
            char *tmp_filepath = dgl_string_c_style(&tmp_builder);
            info->size = sdl_file_size(tmp_filepath);

            dgl_mem_arena_end_temp(temp);
        }
    }
    return(result);
}

ZHC_GET_DATA_BASE_PATH(sdl_internal_storage_path)
{
    bool32 result = false;
#if __ANDROID__
    // NOTE(dgl): On android the asset path is the execution path. Therefore we do nothing.
    // If we put something like . or ./ android does not find the asset folders.
    result = true;
#else
    char *path = SDL_GetBasePath();
    if(path)
    {
        result = true;
#ifdef _WIN32
        dgl_string_append(builder, "%s\\", path);
#else
        dgl_string_append(builder, "%s/", path);
#endif
        SDL_free(path);
    }
    else
    {
        LOG("SDL Get Path failed: %s", SDL_GetError());
    }
#endif

    return(result);
}

ZHC_GET_USER_DATA_BASE_PATH(sdl_external_storage_path)
{
    bool32 result = false;
    #if __ANDROID__
        const char *path = SDL_AndroidGetExternalStoragePath();
    #else
        // NOTE(dgl): we could also use the $HOME or %userprofile% variable
        // but I think, this is currently the best option.
        char *path = SDL_GetPrefPath("co.degit.connect", "Connect");
    #endif
    if(path)
    {
        result = true;
#ifdef _WIN32
        dgl_string_append(builder, "%s\\", path);
#else
        dgl_string_append(builder, "%s/", path);
#endif
#if !__ANDROID__
        SDL_free(path);
#endif
    }

    return(result);
}

ZHC_READ_ENTIRE_FILE(sdl_read_entire_file)
{
    bool32 result = false;

    SDL_RWops *io = SDL_RWFromFile(filename, "rb");
    if(io != 0)
    {
        usize read = SDL_RWread(io, buffer, 1, buffer_size);
        LOG_DEBUG("Reading file %s (%d bytes) into buffer %p (%d bytes)", filename, read, buffer, buffer_size);
        assert(read >= buffer_size, "Could not read entire file");
        result = true;
        SDL_RWclose(io);
    }
    else
    {
        LOG("SDL_RWFromFile failed for %s with error: %s", filename, SDL_GetError());
    }

    return(result);
}

internal int64
get_time_in_ms()
{
    int64 result = 0;
    uint64 frequency = SDL_GetPerformanceFrequency();
    uint64 counter = SDL_GetPerformanceCounter();
    result = int64(counter * 1000 / frequency);
    return(result);
}

int main(int argc, char *argv[])
{
    dgl_log_init(get_time_in_ms);

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        LOG("Unable to initialize SDL: %s", SDL_GetError());
        return(1);
    }
    if(SDLNet_Init() != 0)
    {
        LOG("Unable to initialize SDLNet: %s", SDLNet_GetError());
        return(1);
    }


    SDL_DisableScreenSaver();
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    uint64 target_fps = 30;
    uint64 target_frame_ticks = (SDL_GetPerformanceFrequency() / target_fps);
    real32 target_ms_per_frame = (1.0f / cast(real32)target_fps) * 1000.0f;

    SDL_Window *window =
        SDL_CreateWindow("Connect",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        1024, 768,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_ALLOW_HIGHDPI);

#if ZHC_INTERNAL
    void *base_address = cast(void *)terabytes(2);
#else
    void *base_address = 0;
#endif

    usize permanent_memory_size = megabytes(32);
    usize transient_memory_size = megabytes(16);

    // NOTE(dgl): Must be cleared to zero!!
    void *memory_block = mmap(base_address, permanent_memory_size + transient_memory_size,
                              PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

    if(memory_block != cast(void *)-1)
    {
        void *permanent_memory_block = memory_block;
        void *transient_memory_block = cast(uint8 *)memory_block + permanent_memory_size;

        Zhc_Memory memory = {};
        memory.permanent_storage_size = permanent_memory_size;
        memory.permanent_storage = permanent_memory_block;
        memory.transient_storage_size = transient_memory_size;
        memory.transient_storage = transient_memory_block;
        memory.api.read_entire_file = sdl_read_entire_file;
        memory.api.file_size = sdl_file_size;
        memory.api.get_directory_filenames = get_directory_filenames;
        memory.api.get_data_base_path = sdl_internal_storage_path;
        memory.api.get_user_data_base_path = sdl_external_storage_path;
        memory.api.setup_socket = sdl_net_setup_socket_set;
        memory.api.send_data = sdl_net_send_data;
        memory.api.receive_data = sdl_net_receive_data;

        Zhc_Offscreen_Buffer back_buffer = {};
        Zhc_Input input = {};

        uint64 perf_count_frequency = SDL_GetPerformanceFrequency();
        uint64 last_counter = SDL_GetPerformanceCounter();
        real32 last_frame_in_ms = 0;
        global_running = true;
        while(global_running)
        {
            zhc_input_reset(&input);
            SDL_Event event;

            while(SDL_PollEvent(&event))
            {
                switch(event.type)
                {
                    case SDL_QUIT: { global_running = false; } break;
                    case SDL_TEXTINPUT: { zhc_input_text(&input, event.text.text); } break;
                    case SDL_MOUSEBUTTONDOWN:
                    case SDL_MOUSEBUTTONUP:
                    {
                        bool32 down = (event.type == SDL_MOUSEBUTTONDOWN);

                        if(event.button.button == SDL_BUTTON_LEFT)
                        {
                            zhc_input_mousebutton(&input, Zhc_Mouse_Button_Left, down);
                        }
                        else if(event.button.button == SDL_BUTTON_RIGHT)
                        {
                            zhc_input_mousebutton(&input, Zhc_Mouse_Button_Right, down);
                        }
                        else if(event.button.button == SDL_BUTTON_MIDDLE)
                        {
                            zhc_input_mousebutton(&input, Zhc_Mouse_Button_Middle, down);
                        }
                    } break;
                    case SDL_MOUSEWHEEL:
                    {
                        zhc_input_scroll(&input, v2(event.wheel.x, event.wheel.y));
                    } break;
                    case SDL_MOUSEMOTION:
                    {
                        zhc_input_mousemove(&input, v2(event.motion.x, event.motion.y));
                    } break;
                    case SDL_FINGERMOTION:
                    case SDL_FINGERDOWN:
                    case SDL_FINGERUP:

                    case SDL_KEYDOWN:
                    case SDL_KEYUP:
                    {
                        bool32 down = (event.type == SDL_KEYDOWN);

                        if(event.key.keysym.sym == SDLK_LEFT)
                        {
                            zhc_input_keybutton(&input, Zhc_Keyboard_Button_Left, down);
                        }
                        else if(event.key.keysym.sym == SDLK_RIGHT)
                        {
                            zhc_input_keybutton(&input, Zhc_Keyboard_Button_Right, down);
                        }
                        else if((event.key.keysym.sym == SDLK_RETURN) ||
                                (event.key.keysym.sym == SDLK_RETURN2) ||
                                (event.key.keysym.sym == SDLK_KP_ENTER))
                            zhc_input_keybutton(&input, Zhc_Keyboard_Button_Enter, down);
                    } break;
                    default: {}
                }
            }

            SDL_Surface *surf = SDL_GetWindowSurface(window);
            back_buffer.width = surf->w;
            back_buffer.height = surf->h;
            back_buffer.pitch = surf->pitch;
            back_buffer.bytes_per_pixel = surf->format->BytesPerPixel;
            back_buffer.memory = surf->pixels;

            input.last_frame_in_ms = last_frame_in_ms;

            // TODO(dgl): only render if necessary
            // add render cache to only render rects that have changed
            zhc_update_and_render_server(&memory, &input, &back_buffer);
            SDL_UpdateWindowSurface(window);


            uint64 work_counter = SDL_GetPerformanceCounter();
            uint64 work_ticks_elapsed = work_counter - last_counter;
            uint64 ticks_elapsed = work_ticks_elapsed;
            if(ticks_elapsed < target_frame_ticks)
            {
                real32 ms_per_frame = cast(real32)(1000 * ticks_elapsed) / cast(real32)perf_count_frequency;
                SDL_Delay(cast(uint32)(target_ms_per_frame - ms_per_frame));
                ticks_elapsed = SDL_GetPerformanceCounter() - last_counter;

                // NOTE(dgl): Framerate does not need to be exact.
                /*
                while(ticks_elapsed < target_frame_ticks)
                {
                    ticks_elapsed = SDL_GetPerformanceCounter() - last_counter;
                }
                */
            }


            uint64 end_counter = SDL_GetPerformanceCounter();
            uint64 counter_elapsed = end_counter - last_counter;
            last_frame_in_ms = (((1000.0f * (real32)counter_elapsed) / (real32)perf_count_frequency));

#if 0
            real32 fps = (real32)perf_count_frequency / (real32)counter_elapsed;

            LOG("%.02f ms/f, %.02ff/s", last_frame_in_ms, fps);
#endif
            last_counter = end_counter;
        }

    }

    SDL_EnableScreenSaver();
    return(0);
}
