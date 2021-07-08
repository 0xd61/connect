#ifndef ZHC_PLATFORM_H
#define ZHC_PLATFORM_H

#define DGL_STATIC
#define DGL_DEBUG ZHC_DEBUG
#include "dgl.h"
#define cast(type) dgl_cast(type)

#if __ANDROID__
#include <android/log.h>
#define assert(cond, msg) do \
{                                                                              \
    if (!(cond))                                                               \
    {                                                                          \
      __android_log_print(ANDROID_LOG_ERROR, "co.degit.connect", "Fatal error: %s:%d: dgl_assertion '%s' failed with %s\n",   \
      __FILE__, __LINE__, #cond, #msg);                                        \
      __builtin_trap();                                                        \
    }                                                                          \
} while(0)
#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "co.degit.connect", __VA_ARGS__);
#define LOG_DEBUG(...) __android_log_print(ANDROID_LOG_DEBUG, "co.degit.connect", __VA_ARGS__);
#else
#define assert(cond, msg) dgl_assert(cond, msg)
#define LOG(...) DGL_LOG(__VA_ARGS__)
#define LOG_DEBUG(...) DGL_LOG_DEBUG(__VA_ARGS__)
#endif

#define Stack(Type) struct{usize count; usize offset; Type *memory;}

struct V2
{
    union
    {
        struct { int32 x,y; };
        struct { int32 w,h; };
    };
};

inline V2 v2(int32 x, int32 y)
{
    V2 result = {};
    result.x = x;
    result.y = y;

    return(result);
}

inline V2
v2_add(V2 a, V2 b)
{
    V2 result = {};
    result.x = a.x + b.x;
    result.y = a.y + b.y;

    return(result);
}

struct V4
{
    union
    {
        struct { int32 x,y,w,h; };
        struct { int32 left,top,right,bottom; };
        struct { real32 r,g,b,a; };
    };
};

inline V4
v4(int32 x, int32 y, int32 w, int32 h)
{
    V4 result = {};
    result.x = x;
    result.y = y;
    result.w = w;
    result.h = h;

    return(result);
}

inline V4
v4(real32 r, real32 g, real32 b, real32 a)
{
    V4 result = {};
    result.r = r;
    result.g = g;
    result.b = b;
    result.a = a;

    return(result);
}

struct Zhc_Image
{
    uint32 *pixels;
    int32 width;
    int32 height;
};

struct Zhc_Offscreen_Buffer
{
    int32 width;
    int32 height;
    int32 pitch;
    int32 bytes_per_pixel;
    void *memory;
};

// NOTE(dgl): max 32 supported
enum Zhc_Mouse_Button
{
    Zhc_Mouse_Button_Left = (1 << 0),
    Zhc_Mouse_Button_Middle = (1 << 1),
    Zhc_Mouse_Button_Right = (1 << 2)
};

// NOTE(dgl): max 32 supported
enum Zhc_Keyboard_Button
{
    Zhc_Keyboard_Button_Left = (1 << 0),
    Zhc_Keyboard_Button_Right = (1 << 1),
    Zhc_Keyboard_Button_Enter = (1 << 2)
};

struct Zhc_Input
{
    bool32 has_window_event;
    real32 last_frame_in_ms;

    V2 window_dim;

    V2 pos;
    V2 last_pos;

    V2 scroll_delta;

    // NOTE(dgl): if this flag is set, the element must not be hot
    // before being active. This makes the touch interaction more snappy.
    bool32 force_active_state;

    // NOTE(dgl): only modifier key. We get the text from SDL2.
    int32 key_down;
    int32 key_pressed;
    int32 mouse_down;

    // NOTE(dgl): text from text input event
    char text[32];
};

inline void
zhc_input_keybutton(Zhc_Input *input, Zhc_Keyboard_Button key, bool32 down)
{
    if(down)
    {
        input->key_pressed |= key;
        input->key_down |= key;
    }
    else { input->key_down &= ~key; }
}

inline void
zhc_input_mousebutton(Zhc_Input *input, Zhc_Mouse_Button button, bool32 down)
{
    if(down) { input->mouse_down |= button; }
    else { input->mouse_down &= ~button; }
}

inline void
zhc_input_touch(Zhc_Input *input, Zhc_Mouse_Button button, bool32 down)
{
    // TODO(dgl): The touch event is not really snappy because we need two frames
    // to recognize it. The first frame sets the element to hot and only then we
    // we are able to set the element active in the second frame.
    // However we definately need two frames to determine if the element is on top.
    // We could preserve the touch input for one more frame, if it happens.
    if(down) { input->mouse_down |= button; }
    else { input->mouse_down &= ~button; }
}

inline void
zhc_input_mousemove(Zhc_Input *input, V2 pos)
{
    input->pos = pos;
}

inline void
zhc_input_scroll(Zhc_Input *input, V2 delta)
{
    input->scroll_delta.x += delta.x;
    input->scroll_delta.y += delta.y;
}

inline void
zhc_input_text(Zhc_Input *input, char *text)
{
    usize len = dgl_string_length(input->text);
    usize size = dgl_string_length(text);

    assert(len + size < array_count(input->text), "Text input overflow");
    char *dest = input->text + len;
    dgl_memcpy(dest, text, size);
    dest[size] = '\0';
}

inline void
zhc_input_window(Zhc_Input *input, int32 width, int32 height)
{
    input->window_dim.w = width;
    input->window_dim.h = height;
}

inline void
zhc_input_reset(Zhc_Input *input)
{
    input->text[0] = '\0';
    input->scroll_delta = v2(0, 0);
    input->last_pos = input->pos;
    input->key_pressed = 0;
    input->has_window_event = false;
}

struct Zhc_File_Handle
{
    bool32 no_error;
    void *platform;
};

// TODO(dgl): instead of loading a file by filename and path
// we could use the filehandle. Then we could use the same handle type
// for network and filesystem files.
// This would also allow us to dynamically load and unload files
struct Zhc_File_Info
{
    Zhc_File_Info *next;
    // NOTE(dgl): only filename, without path
    char *filename;
    usize size;
    Zhc_File_Handle handle;
};

struct Zhc_File_Group
{
    int32 count;
    Zhc_File_Info *first_file_info;
    DGL_Mem_Arena *arena;
    // TODO(dgl): not needed. Will always be stored somewhere else.
    // and with the new file handles, we do not neet it to read the
    // file. However we currently use it to reload the file group
    char *dirpath;
};

struct Zhc_Net_Address
{
    union
    {
        struct
        {
            union
            {
                uint8 ip[4];
                uint32 host;
            };
            uint16 port;
        };
        bool32 not_null; /* NOTE(dgl): to make checks if is empty easier */
    };
};

struct Zhc_Net_Socket
{
    Zhc_Net_Address address;
    Zhc_File_Handle handle;
};

// NOTE(dgl): Global api. Use separate api file later...

// TODO(dgl): ZHC_OPEN_FILE
#define ZHC_GET_DIRECTORY_FILENAMES(name) Zhc_File_Group * name(DGL_Mem_Arena *arena, char *path)
typedef ZHC_GET_DIRECTORY_FILENAMES(Zhc_Get_Directory_Filenames);
#define ZHC_CLOSE_FILE(name) void name(Zhc_File_Info *info)
typedef ZHC_CLOSE_FILE(Zhc_Close_File);
#define ZHC_FILE_SIZE(name) usize name(Zhc_File_Handle *handle)
typedef ZHC_FILE_SIZE(Zhc_File_Size);
#define ZHC_READ_ENTIRE_FILE(name) void name(Zhc_File_Handle *handle, uint8 *buffer, usize buffer_size)
typedef ZHC_READ_ENTIRE_FILE(Zhc_Read_Entire_File);
#define ZHC_GET_USER_DATA_BASE_PATH(name) bool32 name(DGL_String_Builder *builder)
typedef ZHC_GET_USER_DATA_BASE_PATH(Zhc_Get_User_Data_Base_Path);
#define ZHC_GET_DATA_BASE_PATH(name) bool32 name(DGL_String_Builder *builder)
typedef ZHC_GET_DATA_BASE_PATH(Zhc_Get_Data_Base_Path);
#define ZHC_OPEN_SOCKET(name) void name(DGL_Mem_Arena *arena, Zhc_Net_Socket *socket)
typedef ZHC_OPEN_SOCKET(Zhc_Open_Socket);
#define ZHC_CLOSE_SOCKET(name) void name(Zhc_Net_Socket *socket)
typedef ZHC_CLOSE_SOCKET(Zhc_Close_Socket);

// TODO(dgl): return peer_address instead of bool32. If there was no data available we could
// return a 0 address (host and port 0).
#define ZHC_RECEIVE_DATA(name) usize name(Zhc_Net_Socket *socket, Zhc_Net_Address *peer_address, uint8 *buffer, usize buffer_size)
typedef ZHC_RECEIVE_DATA(Zhc_Receive_Data);
#define ZHC_SEND_DATA(name) void name(Zhc_Net_Socket *socket, Zhc_Net_Address *target_address, uint8 *buffer, usize buffer_size)
typedef ZHC_SEND_DATA(Zhc_Send_Data);

struct Zhc_Platform_Api
{
    Zhc_Get_Directory_Filenames *get_directory_filenames;
    Zhc_Close_File *close_file;
    Zhc_File_Size *file_size;
    Zhc_Read_Entire_File *read_entire_file;
    Zhc_Get_User_Data_Base_Path *get_user_data_base_path;
    Zhc_Get_Data_Base_Path *get_data_base_path;
    Zhc_Open_Socket *open_socket;
    Zhc_Close_Socket *close_socket;
    Zhc_Receive_Data *receive_data;
    Zhc_Send_Data *send_data;
};

struct Zhc_Memory
{
    Zhc_Platform_Api api;

    usize permanent_storage_size;
    void *permanent_storage; // NOTE(dgl): REQUIRED to be cleared to zero at startup
    usize transient_storage_size;
    void *transient_storage; // NOTE(dgl): REQUIRED to be cleared to zero at startup
};

// NOTE(dgl): zhc_lib.cpp
bool32 zhc_update_and_render_client(Zhc_Memory *memory, Zhc_Input *input, Zhc_Offscreen_Buffer *buffer);
bool32 zhc_update_and_render_server(Zhc_Memory *memory, Zhc_Input *input, Zhc_Offscreen_Buffer *buffer);
#endif // ZHC_PLATFORM_H
