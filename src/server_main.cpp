#include "zhc_platform.h"
// TODO(dgl): should we make this lib a shared lib?
#include "zhc_lib.h"
#include "zhc_lib.cpp"

#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>
#include <SDL.h>
#include <SDL_net.h>
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
#include <errno.h>

#include "sdl2_api.cpp"

global bool32 global_running;

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

    uint64 target_fps = 60;
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

    usize permanent_memory_size = megabytes(64);
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
        memory.api.close_file = sdl_close_file;
        memory.api.file_size = sdl_file_size;
        memory.api.get_directory_filenames = get_directory_filenames;
        memory.api.get_data_base_path = sdl_internal_storage_path;
        memory.api.get_user_data_base_path = sdl_external_storage_path;
        memory.api.open_socket = sdl_net_open_socket;
        memory.api.close_socket = sdl_net_close_socket;
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
#ifdef __ANDROID__
                    case SDL_FINGERMOTION:
                    {
                        int32 x = dgl_round_real32_to_int32(cast(real32)back_buffer.width * event.tfinger.x);
                        int32 y = dgl_round_real32_to_int32(cast(real32)back_buffer.height * event.tfinger.y);
                        zhc_input_mousemove(&input, v2(x, y));
                    } break;
                    case SDL_FINGERDOWN:
                    case SDL_FINGERUP:
                    {
                        bool32 down = (event.type == SDL_FINGERDOWN);
                        zhc_input_mousebutton(&input, Zhc_Mouse_Button_Left, down);
                        int32 x = dgl_round_real32_to_int32(cast(real32)back_buffer.width * event.tfinger.x);
                        int32 y = dgl_round_real32_to_int32(cast(real32)back_buffer.height * event.tfinger.y);
                        zhc_input_mousemove(&input, v2(x, y));

                        if(!down)
                        {
                            // NOTE(dgl): we move the cursor position to 0,0 to reset the "hot" element
                            // IMPORTANT: the mouse click has to be longer than 1 frame. Otherwise it is not
                            // recognized. On a high framerate the user experience should be fine.
                            zhc_input_mousemove(&input, v2(0, 0));
                        }
                    } break;
#else
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
#endif
                    case SDL_TEXTINPUT: { zhc_input_text(&input, event.text.text); } break;
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
                        else if(event.key.keysym.sym == SDLK_UP)
                        {
                            if(down) { zhc_input_scroll(&input, v2(0, 1)); }
                        }
                        else if(event.key.keysym.sym == SDLK_DOWN)
                        {
                            if(down) { zhc_input_scroll(&input, v2(0, -1)); }
                        }
                        else if((event.key.keysym.sym == SDLK_RETURN) ||
                                (event.key.keysym.sym == SDLK_RETURN2) ||
                                (event.key.keysym.sym == SDLK_KP_ENTER))
                            zhc_input_keybutton(&input, Zhc_Keyboard_Button_Enter, down);
                    } break;
                    case SDL_WINDOWEVENT:
                    {
                        input.has_window_event = true;
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
            zhc_input_window(&input, back_buffer.width, back_buffer.height);

            // TODO(dgl): only render if necessary
            // add render cache to only render rects that have changed
            if(zhc_update_and_render_server(&memory, &input, &back_buffer))
            {
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
            uint64 counter_elapsed = end_counter - last_counter;
            last_frame_in_ms = (((1000.0f * (real32)counter_elapsed) / (real32)perf_count_frequency));

#if ZHC_DEBUG
            real32 fps = (real32)perf_count_frequency / (real32)counter_elapsed;

            if(fps < cast(real32)target_fps)
            {
                LOG("FPS DROP %.02f ms/f, %.02ff/s", last_frame_in_ms, fps);
            }
#endif
            last_counter = end_counter;
        }

    }
    else
    {
        LOG("Not enough memory available");
    }

    SDL_EnableScreenSaver();
    return(0);
}
