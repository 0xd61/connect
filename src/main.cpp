#include "zhc_platform.h"
#include "zhc_lib.cpp"

#ifdef __ANDROID__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#define DGL_IMPLEMENTATION
#include "dgl.h"

#include <sys/mman.h> /* mmap */
#include <string.h> /* memset, memcpy */
// NOTE(dgl): On Windows this is only included in the MinGW compiler,
// not in Microsoft Visual C++.
#include <dirent.h> /* opendir, readdir */

global bool32 global_running;

ZHC_FILE_SIZE(sdl_file_size)
{
    usize result = 0;
    SDL_RWops *io = SDL_RWFromFile(filename, "rb");
    int64 size = SDL_RWsize(io);
    assert(size > 0, "Failed to find file");
    result = cast(usize)size;
    SDL_RWclose(io);
    return(result);
}

internal Zhc_File_Info *
allocate_file_info(Zhc_File_Group *group, char *filename, usize filename_size)
{
    Zhc_File_Info *result = dgl_mem_arena_push_struct(group->arena, Zhc_File_Info);
    result->next = group->first_file_info;
    result->filename = dgl_mem_arena_push_array(group->arena, char, filename_size + 1);

    dgl_memcpy(result->filename, filename, filename_size);
    result->filename[filename_size] = '\0';
    group->first_file_info = result;
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
        usize dirpath_count = string_length(path);
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

            usize name_count = string_length(entry->d_name);
            Zhc_File_Info *info = allocate_file_info(result, entry->d_name, name_count);
            DGL_Mem_Temp_Arena temp = dgl_mem_arena_begin_temp(result->arena);

            char *separator = "";
#ifdef _WIN32
            if(path[dirpath_count-1] != '\\') { separator = "\\"; }
#else
            if(path[dirpath_count-1] != '/') { separator = "/"; }
#endif

            usize separator_count = string_length(separator);
            usize filepath_count = dirpath_count + separator_count + name_count;
            char *filepath = dgl_mem_arena_push_array(temp.arena, char, filepath_count + 1);

            // TODO(dgl): better filepath appending.
            void *dest = filepath;
            dgl_memcpy(dest, path, dirpath_count);
            dest = (char *)dest + dirpath_count;
            dgl_memcpy(dest, separator, separator_count);
            dest = (char *)dest + separator_count;
            dgl_memcpy(dest, info->filename, name_count);
            filepath[filepath_count] = '\0';

            info->size = sdl_file_size(filepath);
            dgl_mem_arena_end_temp(temp);
        }
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
    }
    SDL_RWclose(io);

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
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
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

    usize memory_size = megabytes(64);

    void *memory_block = mmap(base_address, memory_size,
                              PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

    if(memory_block != cast(void *)-1)
    {
        Zhc_Memory memory = {};
        memory.storage_size = memory_size;
        memory.storage = memory_block;
        memory.api.read_entire_file = sdl_read_entire_file;
        memory.api.file_size = sdl_file_size;
        memory.api.get_directory_filenames = get_directory_filenames;

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

                        if((event.key.keysym.sym == SDLK_LSHIFT) ||
                           (event.key.keysym.sym == SDLK_RSHIFT))
                        {
                            zhc_input_keybutton(&input, Zhc_Keyboard_Button_Shift, down);
                        }
                        else if((event.key.keysym.sym == SDLK_LCTRL) ||
                                (event.key.keysym.sym == SDLK_RCTRL))
                        {
                            zhc_input_keybutton(&input, Zhc_Keyboard_Button_Ctrl, down);
                        }
                        else if((event.key.keysym.sym == SDLK_LALT) ||
                                (event.key.keysym.sym == SDLK_RALT))
                        {
                            zhc_input_keybutton(&input, Zhc_Keyboard_Button_Alt, down);
                        }
                        else if(event.key.keysym.sym == SDLK_DELETE)
                        {
                            zhc_input_keybutton(&input, Zhc_Keyboard_Button_Del, down);
                        }
                        else if(event.key.keysym.sym == SDLK_BACKSPACE)
                        {
                            zhc_input_keybutton(&input, Zhc_Keyboard_Button_Backspace, down);
                        }
                        else if((event.key.keysym.sym == SDLK_RETURN) ||
                                (event.key.keysym.sym == SDLK_KP_ENTER))
                        {
                            zhc_input_keybutton(&input, Zhc_Keyboard_Button_Enter, down);
                        }
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
            zhc_update_and_render(&memory, &input, &back_buffer);
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
