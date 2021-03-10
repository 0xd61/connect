#include "zhc_lib.h"
#include "zhc_asset.cpp"

#include <string.h>
#include <sys/mman.h> /* mmap */

#define DGL_IMPLEMENTATION
#include "dgl.h"

#include "dgl_test_helpers.h"

int
main(int argc, char **argv)
{
    usize memory_size = megabytes(2);
    uint8 *memory_block = dgl_cast(uint8 *)mmap(0, memory_size,
                              PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

    DGL_Mem_Arena arena = {};
    dgl_mem_arena_init(&arena, memory_block, memory_size);

    DGL_BEGIN_TEST("Init creates general purpose allocator for asset files");
    {
        Zhc_Assets *assets = assets_begin_allocate(&arena, megabytes(1));

        assets_push(assets);
        assets_push(assets);
        Asset_ID asset_id = assets_push(assets);

        assets_end_allocate(assets);

        DGL_EXPECT_int32(asset_id, ==, 2);
        DGL_EXPECT_int32(assets->asset_count, ==, 3);

        Asset_Memory_Block *block = assets->memory_sentinel.next;
        DGL_EXPECT(block->size, ==, megabytes(1) - sizeof(Asset_Memory_Block), usize, "%zu");
        DGL_EXPECT_ptr(block->next, ==, &assets->memory_sentinel);

        dgl_mem_arena_free_all(&arena);
    }
    DGL_END_TEST();

    DGL_BEGIN_TEST("Loading allocates new memory header");
    {
        Zhc_Assets *assets = assets_begin_allocate(&arena, megabytes(1));
        Asset_ID asset_id = assets_push(assets);
        Asset_ID asset_id2 = assets_push(assets);
        assets_end_allocate(assets);

        char *text = "Example Text";
        usize text_length = dgl_string_length(text);
        const usize buffer_size = kilobytes(1023);
        uint8 buffer[buffer_size] = {};

        assets_load_text(assets, asset_id, cast(uint8 *)text, text_length);
        assets_load_text(assets, asset_id2, buffer, buffer_size);

        Loaded_Text *loaded = assets_get_text(assets, asset_id);
        Loaded_Text *loaded2 = assets_get_text(assets, asset_id2);

        DGL_EXPECT(loaded->size, ==, text_length, usize, "%zu");
        DGL_EXPECT(loaded2->size, ==, buffer_size, usize, "%zu");

        Asset_Memory_Block *block = assets->memory_sentinel.next;
        DGL_EXPECT(block->size, ==, text_length + sizeof(Asset_Memory_Header), usize, "%zu");
        DGL_EXPECT(block->next->size, ==, megabytes(1) - text_length - sizeof(Asset_Memory_Header) - 2*sizeof(Asset_Memory_Block), usize, "%zu");

        DGL_EXPECT_ptr(block->next->next, ==, &assets->memory_sentinel);

        dgl_mem_arena_free_all(&arena);
    }
    DGL_END_TEST();

    DGL_BEGIN_TEST("Not loaded assets return 0");
    {
        Zhc_Assets *assets = assets_begin_allocate(&arena, megabytes(1));
        Asset_ID asset_id = assets_push(assets);
        assets_end_allocate(assets);

        Loaded_Text *text = assets_get_text(assets, asset_id);

        DGL_EXPECT_ptr(text, ==, 0);

        dgl_mem_arena_free_all(&arena);
    }
    DGL_END_TEST();

    DGL_BEGIN_TEST("Unloading releases allocations");
    {
        Zhc_Assets *assets = assets_begin_allocate(&arena, megabytes(1));
        Asset_ID asset_id = assets_push(assets);
        assets_end_allocate(assets);

        char *text = "Example Text";
        usize text_length = dgl_string_length(text);
        assets_load_text(assets, asset_id, cast(uint8 *)text, text_length);

        Asset_Memory_Block *block = assets->memory_sentinel.next;
        DGL_EXPECT(block->size, ==, text_length + sizeof(Asset_Memory_Header), usize, "%zu");
        DGL_EXPECT_ptr(block->next->next, ==, &assets->memory_sentinel);

        assets_unload(assets, asset_id);

        block = assets->memory_sentinel.next;
        DGL_EXPECT_ptr(block->next, ==, &assets->memory_sentinel);
        DGL_EXPECT(block->size, ==, megabytes(1) - sizeof(Asset_Memory_Block), usize, "%zu");

        dgl_mem_arena_free_all(&arena);
    }
    DGL_END_TEST();

    DGL_BEGIN_TEST("Small blocks do not get splitted");
    {
        Zhc_Assets *assets = assets_begin_allocate(&arena, kilobytes(1));
        Asset_ID asset_id = assets_push(assets);
        assets_end_allocate(assets);

        char *text = "Example Text";
        usize text_length = dgl_string_length(text);
        assets_load_text(assets, asset_id, cast(uint8 *)text, text_length);

        Asset_Memory_Block *block = assets->memory_sentinel.next;
        DGL_EXPECT(block->size, ==, kilobytes(1) - sizeof(Asset_Memory_Block), usize, "%zu");
        DGL_EXPECT_ptr(block->next, ==, &assets->memory_sentinel);

        // NOTE(dgl): Last block does not get merged
        assets_unload(assets, asset_id);
        block = assets->memory_sentinel.next;
        DGL_EXPECT_ptr(block->next, ==, &assets->memory_sentinel);

        dgl_mem_arena_free_all(&arena);
    }
    DGL_END_TEST();

    DGL_BEGIN_TEST("Allocating multiple blocks");
    {
        Zhc_Assets *assets = assets_begin_allocate(&arena, kilobytes(5));
        Asset_ID asset_id1 = assets_push(assets);
        Asset_ID asset_id2 = assets_push(assets);
        Asset_ID asset_id3 = assets_push(assets);
        Asset_ID asset_id4 = assets_push(assets);
        Asset_ID asset_id5 = assets_push(assets);
        assets_end_allocate(assets);

        char *text1 = "Example Text1";
        usize text_length1 = dgl_string_length(text1);

        char *text2 = "Example Text22";
        usize text_length2 = dgl_string_length(text2);

        char *text3 = "Example Text333";
        usize text_length3 = dgl_string_length(text3);

        char *text4 = "Example Text4444";
        usize text_length4 = dgl_string_length(text4);

        char *text5 = "Example Text5";
        usize text_length5 = dgl_string_length(text5);

        assets_load_text(assets, asset_id1, cast(uint8 *)text1, text_length1);
        assets_load_text(assets, asset_id2, cast(uint8 *)text2, text_length2);
        assets_load_text(assets, asset_id3, cast(uint8 *)text3, text_length3);

        Asset_Memory_Block *block = assets->memory_sentinel.next;
        DGL_EXPECT(block->size, ==, text_length1 + sizeof(Asset_Memory_Header), usize, "%zu");
        DGL_EXPECT(block->next->size, ==, text_length2 + sizeof(Asset_Memory_Header), usize, "%zu");
        DGL_EXPECT(block->next->next->size, ==, text_length3 + sizeof(Asset_Memory_Header), usize, "%zu");

        assets_unload(assets, asset_id2);
        DGL_EXPECT(block->next->size, ==, text_length2 + sizeof(Asset_Memory_Header), usize, "%zu");
        DGL_EXPECT_bool32(block->next->used, ==, false);

        usize remaining_size = block->next->next->next->size;
        assets_load_text(assets, asset_id4, cast(uint8 *)text4, text_length4);
        DGL_EXPECT(block->next->next->next->size, ==, remaining_size, usize, "%zu");
        DGL_EXPECT_bool32(block->next->used, ==, false);

        assets_load_text(assets, asset_id5, cast(uint8 *)text5, text_length5);
        Loaded_Text *loaded5 = assets_get_text(assets, asset_id5);
        DGL_EXPECT_bool32(block->next->used, ==, true);
        DGL_EXPECT(block->next->size, ==, text_length2 + sizeof(Asset_Memory_Header), usize, "%zu");
        DGL_EXPECT_ptr(block->next->next->next->next, ==, &assets->memory_sentinel);
        DGL_EXPECT_int32(strncmp(text5, cast(char *)loaded5->memory, text_length5), ==, 0);

        dgl_mem_arena_free_all(&arena);
    }
    DGL_END_TEST();

    if(dgl_test_result()) { return(0); }
    else { return(1); }
}
