#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <sys/mman.h> /* mmap */

#include "zhc_platform.h"
#include <dirent.h> /* opendir, readdir */
#include <errno.h>
#include "sdl2_api.cpp"

#define DGL_IMPLEMENTATION
#include "dgl.h"

#include "dgl_test_helpers.h"
#include <unistd.h>

int
main(int argc, char **argv)
{
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        LOG("Unable to initialize SDL: %s", SDL_GetError());
        return(1);
    }
    if(SDLNet_Init() != 0)
    {
        LOG("Unable to initialize SDLNet: %s", SDLNet_GetError());
        return(1);
    }

    usize memory_size = kilobytes(32);
    uint8 *memory_block = dgl_cast(uint8 *)mmap(0, memory_size,
                              PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

    DGL_Mem_Arena arena = {};
    dgl_mem_arena_init(&arena, memory_block, memory_size);

    DGL_BEGIN_TEST("Loading a directory, loads all files ordered by filename into a filegroup");
    {
        Zhc_File_Group *group = get_directory_filenames(&arena, "../data/files");

        Zhc_File_Info *info = group->first_file_info;
        DGL_EXPECT_int32(strcmp(info->filename, "01-first_file.txt"), ==, 0);
        info = info->next;
        DGL_EXPECT_int32(strcmp(info->filename, "02-photograph.txt"), ==, 0);
        info = info->next;
        DGL_EXPECT_int32(strcmp(info->filename, "03-tabs.txt"), ==, 0);
        DGL_EXPECT_ptr(info->next, ==, 0);

        DGL_EXPECT_int32(strcmp(group->dirpath, "../data/files/"), ==, 0);

        dgl_mem_arena_free_all(&arena);
    }
    DGL_END_TEST();

    DGL_BEGIN_TEST("Directory group path always ends with a \\ or /");
    {
        Zhc_File_Group *group = get_directory_filenames(&arena, "../data/files");
        DGL_EXPECT_int32(strcmp(group->dirpath, "../data/files/"), ==, 0);
        Zhc_File_Group *group1 = get_directory_filenames(&arena, "../data/files/");
        DGL_EXPECT_int32(strcmp(group1->dirpath, "../data/files/"), ==, 0);
    }
    DGL_END_TEST();

    if(dgl_test_result()) { return(0); }
    else { return(1); }
}
