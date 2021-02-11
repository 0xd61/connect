#include "zhc_platform.h"
#include "zhc_lib.cpp"
#include "zhc_renderer.cpp"

#ifdef __ANDROID__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#define DGL_IMPLEMENTATION
#include "dgl.h"

#include <sys/mman.h> /* mmap */
#include <string.h> /* memset, memcpy */

global bool32 global_running;

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

    uint64 target_frame_rate = 10;
    uint64 target_frame_ticks = (target_frame_rate * SDL_GetPerformanceFrequency()) / 1000;
    real32 target_ms_per_frame = (1.0f / cast(real32)target_frame_rate) * 1000.0f;

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

    usize memory_size = megabytes(4);

    void *memory_block = mmap(base_address, memory_size,
                              PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

    if(memory_block != cast(void *)-1)
    {
        Zhc_Memory memory = {};
        memory.update_storage_size = memory_size / 2;
        memory.render_storage_size = memory_size - memory.update_storage_size;
        memory.update_storage = memory_block;
        memory.render_storage = cast(uint8 *)memory_block + memory.update_storage_size;
        memory.api.read_entire_file = sdl_read_entire_file;
        memory.api.file_size = sdl_file_size;

        Zhc_Offscreen_Buffer back_buffer = {};
        Zhc_Input input = {};

        // TODO(dgl): maybe it makes more sense to pass the buffer to each render call.
        // I think this solution is confusing.
        zhc_render_init(&memory, &back_buffer);

        uint64 perf_count_frequency = SDL_GetPerformanceFrequency();
        uint64 last_counter = SDL_GetPerformanceCounter();
        global_running = true;
        while(global_running)
        {
            zhc_input_reset(&input);
            SDL_Event event;
            bool32 rerender = false;
            while(SDL_PollEvent(&event))
            {
                rerender = true;
                switch(event.type)
                {
                    case SDL_QUIT: { global_running = false; } break;
                    case SDL_WINDOWEVENT:
                    {
                        if(event.window.event == SDL_WINDOWEVENT_RESIZED)
                        {
                            zhc_window_resize(&input, v2(event.window.data1, event.window.data2));
                        }
                    } break;
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
            zhc_window_resize(&input, v2(back_buffer.width, back_buffer.height));

            if(rerender)
            {
                zhc_update(&memory, &input);

                Zhc_Command *cmd = 0;
                while(zhc_next_command(&memory, &cmd))
                {
                    switch(cmd->type)
                    {
                        case Command_Type_Rect:
                        {
                            zhc_render_rect(&memory, cmd->rect_cmd.rect, cmd->rect_cmd.color);
                        } break;
                        case Command_Type_Text:
                        {
                            zhc_render_text(&memory, cmd->text_cmd.rect, cmd->text_cmd.color, cast(char *)cmd->text_cmd.text);
                        } break;
                        default:
                        {
                            LOG("Command type not supported");
                        }
                    }
                }

                SDL_UpdateWindowSurface(window);
            }

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

#if 0
            uint64 counter_elapsed = end_counter - last_counter;
            real32 ms_per_frame = (((1000.0f * (real32)counter_elapsed) / (real32)perf_count_frequency));
            real32 fps = (real32)perf_count_frequency / (real32)counter_elapsed;

            LOG("%.02f ms/f, %.02ff/s", ms_per_frame, fps);
#endif
            last_counter = end_counter;
        }

    }

    SDL_EnableScreenSaver();
    return(0);
}
