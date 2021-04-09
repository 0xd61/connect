// NOTE(dgl): Platform specific API implementations for the server and client platform.
// DO NOT INCLUDE THIS FILE INTO THE PLATFORM INDEPENDENT CODE!

// NOTE(dgl): to bind the udp socket to a random port, use a empty
// Zhc_Net_Address structure.
ZHC_OPEN_SOCKET(sdl_net_open_socket)
{
    UDPsocket sock = SDLNet_UDP_Open(socket->address.port);
    if(sock)
    {
        socket->handle.platform = sock;
        socket->handle.no_error = true;
    }
}

ZHC_CLOSE_SOCKET(sdl_net_close_socket)
{
    if(socket->handle.platform)
    {
        UDPsocket udpsock = (UDPsocket)socket->handle.platform;
        SDLNet_UDP_Close(udpsock);
        socket->handle.platform = 0;
        socket->handle.no_error = 0;
    }
}

ZHC_SEND_DATA(sdl_net_send_data)
{
    UDPpacket packet;
    packet.data = cast(Uint8 *)buffer;
    packet.maxlen = dgl_safe_size_to_int32(buffer_size);
    packet.len = dgl_safe_size_to_int32(buffer_size);
    packet.address.host = target_address->host;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    packet.address.port = SDL_Swap16(target_address->port);
#else
    packet.address.port = target_address->port;
#endif

    UDPsocket udp_sock = (UDPsocket)socket->handle.platform;
    int32 success = SDLNet_UDP_Send(udp_sock, -1, &packet);
    if(success == 0)
    {
        LOG("Failed sending udp package: %s", SDLNet_GetError());
        socket->handle.no_error = false;
    }
}

ZHC_RECEIVE_DATA(sdl_net_receive_data)
{
    int32 result = 0;
    UDPpacket packet;
    packet.data = cast(Uint8 *)buffer;
    packet.maxlen = dgl_safe_size_to_int32(buffer_size);

    if(socket->handle.no_error)
    {
        UDPsocket udp_sock = (UDPsocket)socket->handle.platform;
        int32 success =SDLNet_UDP_Recv(udp_sock, &packet);
        if(success > 0)
        {
            result = packet.len;

            peer_address->host = packet.address.host;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
            peer_address->port = SDL_Swap16(packet.address.port);
#else
            peer_address->port = packet.address.port;
#endif

        }
        else if(success < 0)
        {
            LOG("Failed receiving udp package: %s", SDLNet_GetError());
            socket->handle.no_error = false;
        }
    }
    return(result);
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
