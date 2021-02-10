#define DGL_STATIC
#define DGL_DEBUG ZHC_DEBUG
#define DGL_IMPLEMENTATION
#include "dgl.h"
#define assert(cond, msg) dgl_assert(cond, msg)
#define cast(type) dgl_cast(type)
#define LOG(...) DGL_LOG(__VA_ARGS__)
#define LOG_DEBUG(...) DGL_LOG_DEBUG(__VA_ARGS__)

#include "zhc_types.h"
#include "zhc_lib.cpp"

#ifdef __ANDROID__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#include "zhc_renderer.cpp"

#include <sys/mman.h> /* mmap */
#include <string.h> /* memset, memcpy */

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

internal int32
string_length(char *s)
{
    int32 result = 0;
    while(*s++) { ++result; }

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

    uint64 target_frame_rate = 24;
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
        memory.storage_size = memory_size;
        memory.storage = memory_block;

        Zhc_Renderer renderer = {};
        zhc_render_init(&renderer, window);

        uint64 perf_count_frequency = SDL_GetPerformanceFrequency();
        uint64 last_counter = SDL_GetPerformanceCounter();

        global_running = true;
        while(global_running)
        {
            SDL_Event event;
            bool32 rerender = false;
            while(SDL_PollEvent(&event))
            {
                rerender = true;
                switch(event.type)
                {
                    case SDL_QUIT: { global_running = false; } break;
                }
            }

            if(rerender)
            {
                zhc_update(&memory);

                Zhc_Command *cmd = 0;
                while(zhc_next_command(&memory, &cmd))
                {
                    // TODO(dgl): render based on command_list
                    //zhc_render_rect(&renderer);
                }
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

#if 1
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
