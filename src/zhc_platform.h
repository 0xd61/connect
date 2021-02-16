#ifndef ZHC_PLATFORM_H
#define ZHC_PLATFORM_H

#define DGL_STATIC
#define DGL_DEBUG ZHC_DEBUG
#include "dgl.h"
#define assert(cond, msg) dgl_assert(cond, msg)
#define cast(type) dgl_cast(type)
#define LOG(...) DGL_LOG(__VA_ARGS__)
#define LOG_DEBUG(...) DGL_LOG_DEBUG(__VA_ARGS__)

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
rect(int32 x, int32 y, int32 w, int32 h)
{
    V4 result = {};
    result.x = x;
    result.y = y;
    result.w = w;
    result.h = h;

    return(result);
}

inline V4
color(real32 r, real32 g, real32 b, real32 a)
{
    V4 result = {};
    result.r = r;
    result.g = g;
    result.b = b;
    result.a = a;

    return(result);
}

enum Zhc_Command_Type
{
    Command_Type_Rect,
    Command_Type_Text,
};

struct Zhc_Rect_Command
{
    V4 rect;
    V4 color;
};

struct Zhc_Text_Command
{
    V4 rect;
    V4 color;
    uint8 text[1];
};

struct Zhc_Command
{
    Zhc_Command_Type type;
    int32 size;
    union
    {
        Zhc_Text_Command text_cmd;
        Zhc_Rect_Command rect_cmd;
    };
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
    Zhc_Keyboard_Button_Shift = (1 << 0),
    Zhc_Keyboard_Button_Ctrl = (1 << 1),
    Zhc_Keyboard_Button_Alt = (1 << 2),
    Zhc_Keyboard_Button_Del = (1 << 3),
    Zhc_Keyboard_Button_Backspace = (1 << 4),
    Zhc_Keyboard_Button_Enter = (1 << 5)
};

struct Zhc_Input
{
    real32 last_frame_in_ms;
    V2 window;

    V2 pos;
    V2 last_pos;

    V2 scroll_delta;

    // NOTE(dgl): only modifier key. We get the text from SDL2.
    int32 key_down;
    int32 mouse_down;

    // NOTE(dgl): text from text input event
    char text[32];
};

struct Zhc_File_Info
{
    Zhc_File_Info *next;
    // NOTE(dgl): only filename, without path
    char *filename;
    usize size;
};

// NOTE(dgl): files are stored in reverse order.
struct Zhc_File_Group
{
    int32 count;
    Zhc_File_Info *first_file_info;
    DGL_Mem_Arena *arena;
    char *dirpath;
};

inline void
zhc_input_keybutton(Zhc_Input *input, Zhc_Keyboard_Button key, bool32 down)
{
    if(down) { input->key_down |= key; }
    else { input->key_down &= ~key; }
}

inline void
zhc_input_mousebutton(Zhc_Input *input, Zhc_Mouse_Button button, bool32 down)
{
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
zhc_window_resize(Zhc_Input *input, V2 dim)
{
    input->window = dim;
}

internal usize
string_length(char *s)
{
    usize result = 0;
    while(*s++) { ++result; }

    return(result);
}

inline void
zhc_input_text(Zhc_Input *input, char *text)
{
    usize len = string_length(input->text);
    usize size = string_length(text);

    assert(len + size < array_count(input->text), "Text input overflow");
    char *dest = input->text + len;
    dgl_memcpy(dest, text, size);
    dest[size] = '\0';
}

inline void
zhc_input_reset(Zhc_Input *input)
{
    input->key_down = 0;
    input->text[0] = '\0';
    input->mouse_down = 0;
    input->scroll_delta = v2(0, 0);
    input->last_pos = input->pos;
}

// NOTE(dgl): Global api. Use separate api file later...
#define ZHC_GET_DIRECTORY_FILENAMES(name) Zhc_File_Group * name(DGL_Mem_Arena *arena, char *path)
typedef ZHC_GET_DIRECTORY_FILENAMES(Zhc_Get_Directory_Filenames);
#define ZHC_FILE_SIZE(name) usize name(char *filename)
typedef ZHC_FILE_SIZE(Zhc_File_Size);
#define ZHC_READ_ENTIRE_FILE(name) bool32 name(char *filename, uint8 *buffer, usize buffer_size)
typedef ZHC_READ_ENTIRE_FILE(Zhc_Read_Entire_File);
struct Zhc_Platform_Api
{
    Zhc_Get_Directory_Filenames *get_directory_filenames;
    Zhc_File_Size *file_size;
    Zhc_Read_Entire_File *read_entire_file;
};

struct Zhc_Memory
{
    Zhc_Platform_Api api;

    usize update_storage_size;
    void *update_storage; // NOTE(dgl): REQUIRED to be cleared to zero at startup

    usize render_storage_size;
    void *render_storage; // NOTE(dgl): REQUIRED to be cleared to zero at startup
};

// NOTE(dgl): zhc_lib.cpp
void zhc_update(Zhc_Memory *memory, Zhc_Input *input);
bool32 zhc_next_command(Zhc_Memory *memory, Zhc_Command **cmd);

// NOTE(dgl): zhc_renderer.cpp
void zhc_render_init(Zhc_Memory *memory, Zhc_Offscreen_Buffer *buffer);
void zhc_render_rect(Zhc_Memory *memory, V4 rect, V4 color);
void zhc_render_text(Zhc_Memory *memory, V4 rect, V4 color, char *text);

#endif // ZHC_PLATFORM_H
