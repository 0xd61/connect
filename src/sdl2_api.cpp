// NOTE(dgl): Platform specific API implementations for the server and client platform.
// DO NOT INCLUDE THIS FILE INTO THE PLATFORM INDEPENDENT CODE!

struct SDL_Client_Socket
{
    TCPsocket socket;
};

// NOTE(dgl): client structure stored inside the server array
struct SDL_Client
{
    IPaddress peer;
    TCPsocket socket;
};

#define MAX_CLIENTS 64
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

ZHC_SEND_DATA(sdl_net_server_send_data)
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

ZHC_RECEIVE_DATA(sdl_net_server_receive_data)
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

                        peer_address->host = client->peer.host;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
                        peer_address->port = SDL_Swap16(client->peer.port);
#else
                        peer_address->port = client->peer.port;
#endif

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

ZHC_SETUP_SOCKET(sdl_net_server_setup_socket_set)
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

ZHC_SEND_DATA(sdl_net_client_send_data)
{
    assert(target_address, "Target address cannot be null");
    SDL_Client_Socket *platform = (SDL_Client_Socket *)socket->platform;
    int32 result = SDLNet_TCP_Send(platform->socket, buffer, dgl_safe_size_to_int32(buffer_size));
    if(result <= 0)
    {
        SDLNet_TCP_Close(platform->socket);
        socket->no_error = false;
    }
    else
    {
        LOG_DEBUG("Sending %d bytes", result);
    }
}

ZHC_RECEIVE_DATA(sdl_net_client_receive_data)
{
    bool32 result = false;
    SDL_Client_Socket *platform = (SDL_Client_Socket *)socket->platform;

    if(peer_address->port > 0 && peer_address->host > 0)
    {
        IPaddress *real_peer = SDLNet_TCP_GetPeerAddress(platform->socket);
        if(is_ip_address(*peer_address, *real_peer))
        {
            int32 success = SDLNet_TCP_Recv(platform->socket, buffer, dgl_safe_size_to_int32(buffer_size));
            if(success <= 0)
            {
                SDLNet_TCP_Close(platform->socket);
                socket->no_error = false;
            }
        }
    }
    else
    {
        int32 success = SDLNet_TCP_Recv(platform->socket, buffer, dgl_safe_size_to_int32(buffer_size));
        if(success <= 0)
        {
            SDLNet_TCP_Close(platform->socket);
            socket->no_error = false;
        }
        else
        {
            LOG_DEBUG("Receiving %d bytes", success);
        }
    }
    return(result);
}

ZHC_SETUP_SOCKET(sdl_net_client_setup_socket)
{
    IPaddress ip = {};
    ip.host = socket->address.host;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    ip.port = SDL_Swap16(socket->address.port);
#else
    ip.port = socket->address.port;
#endif

    TCPsocket tcpsock = SDLNet_TCP_Open(&ip);
    if(tcpsock)
    {
        SDL_Client_Socket *client_socket = dgl_mem_arena_push_struct(arena, SDL_Client_Socket);
        client_socket->socket = tcpsock;

        socket->platform = client_socket;
        socket->no_error = true;
    }
    else
    {
        LOG("Opening TCP socket failed with: %s", SDLNet_GetError());
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
        LOG_DEBUG("SDL_RWFromFile failed for %s with error: %s", filename, SDL_GetError());
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

        // NOTE(dgl): ensure the dirpath ends with a \ or /
#ifdef DGL_OS_WINDOWS
        if(path[dirpath_count - 1] != '\\')
        {
            dirpath_count++;
        }
        result->dirpath = dgl_mem_arena_push_array(arena, char, dirpath_count + 1);
        dgl_memcpy(result->dirpath, path, dirpath_count);
        result->dirpath[dirpath_count - 1] = '\\';
        result->dirpath[dirpath_count] = '\0';
#else
        if(path[dirpath_count - 1] != '/')
        {
            dirpath_count++;
        }
        result->dirpath = dgl_mem_arena_push_array(arena, char, dirpath_count + 1);
        dgl_memcpy(result->dirpath, path, dirpath_count);
        result->dirpath[dirpath_count - 1] = '/';
        result->dirpath[dirpath_count] = '\0';
#endif

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
            dgl_string_append(&tmp_builder, "%s%s", result->dirpath, info->filename);

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
#if DGL_OS_WINDOWS
        dgl_string_append(builder, "%s\\", path);
#else
        dgl_string_append(builder, "%s/", path);
#endif
        SDL_free(path);
    }
    else
    {
        LOG_DEBUG("SDL Get Path failed: %s", SDL_GetError());
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
#ifdef DGL_OS_WINDOWS
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
        LOG_DEBUG("SDL_RWFromFile failed for %s with error: %s", filename, SDL_GetError());
    }

    return(result);
}
