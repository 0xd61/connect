// NOTE(dgl): Platform specific API implementations for the server and client platform.
// DO NOT INCLUDE THIS FILE INTO THE PLATFORM INDEPENDENT CODE!

struct SDL_Client_Socket
{
    SDLNet_SocketSet set;
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
                        if(!peer_address->not_null)
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

        result = ready_count > 0;
    }
    else if(ready_count < 0)
    {
        LOG("Checking ready sockets failed with: %s", SDLNet_GetError());
    }

    return(result);
}

ZHC_OPEN_SOCKET(sdl_net_server_setup_socket_set)
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

    int32 ready_count = SDLNet_CheckSockets(platform->set, 0);
    if(ready_count > 0)
    {
        // NOTE(dgl): handle new incoming connecitons
        if(SDLNet_SocketReady(platform->socket))
        {
            --ready_count;

            if(peer_address->not_null)
            {
                // NOTE(dgl): early return if the ip address is different than the peers
                // address
                IPaddress *real_peer = SDLNet_TCP_GetPeerAddress(platform->socket);
                if(!is_ip_address(*peer_address, *real_peer))
                {
                    LOG_DEBUG("Ip address of peer does not match");
                    result = true;
                    return(result);
                }
            }

            int32 success = SDLNet_TCP_Recv(platform->socket, buffer, dgl_safe_size_to_int32(buffer_size));
            if(success <= 0)
            {
                LOG_DEBUG("Failed to receive data with %s", SDLNet_GetError());
                SDLNet_TCP_Close(platform->socket);
                socket->no_error = false;
            }
            else
            {
                LOG_DEBUG("Receiving %d bytes", success);
            }
        }

        result = ready_count > 0;
    }
    else if(ready_count < 0)
    {
        LOG("Checking ready sockets failed with: %s", SDLNet_GetError());
    }

    return(result);
}

// NOTE(dgl): I was not able to set the sockets to O_NONBLOCK.
// therefore we have to create a socket set and check if data
// is available. I guess this is overall the better solution.
ZHC_OPEN_SOCKET(sdl_net_client_setup_socket)
{
    IPaddress ip = {};
    ip.host = socket->address.host;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    ip.port = SDL_Swap16(socket->address.port);
#else
    ip.port = socket->address.port;
#endif

    SDLNet_SocketSet set = SDLNet_AllocSocketSet(1);
    if(set)
    {
        TCPsocket tcpsock = SDLNet_TCP_Open(&ip);
        if(tcpsock)
        {
            SDLNet_TCP_AddSocket(set, tcpsock);
            SDL_Client_Socket *client_socket = dgl_mem_arena_push_struct(arena, SDL_Client_Socket);
            client_socket->socket = tcpsock;
            client_socket->set = set;

            socket->platform = client_socket;
            socket->no_error = true;
        }
        else
        {
            LOG("Opening TCP socket failed with: %s", SDLNet_GetError());
        }
    }
    else
    {
        LOG("Allocating socket failed with: %s", SDLNet_GetError());
    }
}

ZHC_FILE_SIZE(sdl_file_size)
{
    usize result = 0;
    if(handle->no_error)
    {
        SDL_RWops *io = cast(SDL_RWops *)handle->platform;
        int64 size = SDL_RWsize(io);
        if(size < 0)
        {
            handle->no_error = false;
            LOG_DEBUG("Failed to find filesize");
        }
        result = cast(usize)size;
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

    DGL_Mem_Temp_Arena temp = dgl_mem_arena_begin_temp(group->arena);
    {
        DGL_String_Builder tmp_builder = dgl_string_builder_init(temp.arena, 128);
        dgl_string_append(&tmp_builder, "%s%s", group->dirpath, result->filename);

        char *tmp_filepath = dgl_string_c_style(&tmp_builder);
        SDL_RWops *io = cast(SDL_RWops *)SDL_RWFromFile(tmp_filepath, "rb");
        if(io != 0)
        {
            result->handle.no_error = true;
            result->handle.platform = io;
        }
        else
        {
            LOG_DEBUG("SDL_RWFromFile failed for %s with error: %s", tmp_filepath, SDL_GetError());
        }
    }
    dgl_mem_arena_end_temp(temp);

    group->count++;

    return(result);
}

#if __ANDROID__
ZHC_GET_DIRECTORY_FILENAMES(android_get_asset_directory_filenames)
{
    Zhc_File_Group *result = 0;

    // NOTE(dgl): get asset manager (same as Internal_Android_Create_AssetManager in SDL_android.c)
    // No idea if this is the correy way. But it works for now.
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    assert(env, "Failed to get env");

    // retrieve the Java instance of the SDLActivity
    jobject activity = (jobject)SDL_AndroidGetActivity();
    assert(activity, "Failed to get activity");

    // find the Java class of the activity. It should be SDLActivity or a subclass of it.
    jclass activity_class = env->GetObjectClass(activity);

    jmethodID midGetContext = env->GetStaticMethodID(activity_class, "getContext","()Landroid/content/Context;");

    /* context = SDLActivity.getContext(); */
    jobject context = env->CallStaticObjectMethod(activity_class, midGetContext);
    jclass context_class = env->GetObjectClass(context);

    /* javaAssetManager = context.getAssets(); */
    jmethodID midAssetManager = env->GetMethodID(context_class, "getAssets", "()Landroid/content/res/AssetManager;");
    jobject javaAssetManager = env->CallObjectMethod(context, midAssetManager);
    jobject javaAssetManagerRef = env->NewLocalRef(javaAssetManager);
    AAssetManager *asset_manager = AAssetManager_fromJava(env, javaAssetManagerRef);

    assert(asset_manager, "Could not create an asset manager");

    // NOTE(dgl): retrieving the files of the asset directory with the android asset
    // manager
    AAssetDir *dir = AAssetManager_openDir(asset_manager, path);
    if(dir)
    {
        result = dgl_mem_arena_push_struct(arena, Zhc_File_Group);
        result->arena = arena;

        usize dirpath_count = dgl_string_length(path);
        if(path[dirpath_count - 1] != '/')
        {
            dirpath_count++;
        }
        result->dirpath = dgl_mem_arena_push_array(arena, char, dirpath_count + 1);
        dgl_memcpy(result->dirpath, path, dirpath_count);
        result->dirpath[dirpath_count - 1] = '/';
        result->dirpath[dirpath_count] = '\0';

        const char *entry = AAssetDir_getNextFileName(dir);
        while(entry)
        {
            if(entry[0] == '.' &&
               entry[1] == '\0'){ continue; }

            if(entry[0] == '.' &&
               entry[1] == '.' &&
               entry[2] == '\0'){ continue; }

            LOG_DEBUG("Found file: %s", entry);

            Zhc_File_Info *info = allocate_file_info(result, cast(char *)entry);
            info->size = sdl_file_size(&info->handle);

            entry = AAssetDir_getNextFileName(dir);
        }

        AAssetDir_close(dir);
    }
    else
    {
        LOG("Failed opening path: %s", path);
    }

    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(activity_class);
    env->DeleteLocalRef(context);
    env->DeleteLocalRef(context_class);
    env->DeleteLocalRef(javaAssetManager);
    env->DeleteLocalRef(javaAssetManagerRef);

    return(result);
}
#else
ZHC_GET_DIRECTORY_FILENAMES(get_directory_filenames)
{
    Zhc_File_Group *result = 0;

    // TODO(dgl): get android assets path

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

            LOG_DEBUG("Found file: %s", entry->d_name);

            Zhc_File_Info *info = allocate_file_info(result, entry->d_name);
            info->size = sdl_file_size(&info->handle);
        }
    }
    else
    {
        LOG("Failed opening path: %s with error: %d, %s", path, errno, strerror(errno));
    }


    return(result);
}
#endif // __ANDROID__

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
    if(handle->no_error)
    {
        SDL_RWops *io = cast(SDL_RWops *)handle->platform;
        usize read = SDL_RWread(io, buffer, sizeof(*buffer), buffer_size);
        LOG_DEBUG("Reading (%d bytes) from file into buffer %p (%d bytes)", read, buffer, buffer_size);
        if(read == buffer_size)
        {
            if(SDL_RWseek(io, 0, RW_SEEK_SET) < 0)
            {
                handle->no_error = false;
                LOG_DEBUG("Could not set the file cursor to beginning of file after reading.");
            }
        }
        else
        {
            handle->no_error = false;
            LOG_DEBUG("Could not read entire file: Handle: %p, Buffer: %p, Size: %zu, Read: %zu", io, buffer, buffer_size, read);
        }
    }
}
