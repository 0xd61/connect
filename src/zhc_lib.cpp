#define DGL_STATIC
#define DGL_DEBUG ZHC_DEBUG
#include "dgl.h"
#define assert(cond, msg) dgl_assert(cond, msg)
#define cast(type) dgl_cast(type)
#define LOG(...) DGL_LOG(__VA_ARGS__)
#define LOG_DEBUG(...) DGL_LOG_DEBUG(__VA_ARGS__)

#include "zhc_types.h"
#include "zhc_input.cpp"

enum Zhc_Command_Type
{
    Command_Type_Rect,
    Command_Type_Text,
};

struct Zhc_Command
{
    Zhc_Command_Type type;
    int32 size;
    uint8 data[1];
};

struct Zhc_Memory
{
    usize storage_size;
    void *storage; // NOTE(dgl): REQUIRED to be cleared to zero at startup
};

struct Lib_State
{
    DGL_Mem_Arena permanent_arena;
    DGL_Mem_Arena transient_arena; // NOTE(dgl): cleared on each frame

    usize command_list_offset;
    usize command_list_size;
    uint8 *command_list;

    bool32 is_initialized;
};

void
zhc_update(Zhc_Memory *memory, Zhc_Input *input)
{
    assert(sizeof(Lib_State) < memory->storage_size, "Not enough memory allocated");

    Lib_State *state = cast(Lib_State *)memory->storage;
    if(!state->is_initialized)
    {
        LOG_DEBUG("Lib_State size: %lld, Available memory: %lld", sizeof(*state), memory->storage_size);
        dgl_mem_arena_init(&state->permanent_arena, (uint8 *)memory->storage + sizeof(*state), ((DGL_Mem_Index)memory->storage_size - sizeof(*state))/2);
        dgl_mem_arena_init(&state->transient_arena, state->permanent_arena.base + state->permanent_arena.size, (DGL_Mem_Index)memory->storage_size - state->permanent_arena.size);

        state->is_initialized = true;
    }

    // NOTE(dgl): clear transient arena for new frame.
    dgl_mem_arena_free_all(&state->transient_arena);

    LOG_DEBUG("Current Text: %s", input->text);



}

// NOTE(dgl): call this function only after zhc_update!!
bool32
zhc_next_command(Zhc_Memory *memory, Zhc_Command **cmd)
{
    Lib_State *state = cast(Lib_State *)memory->storage;
    assert(state->is_initialized, "Initialize the lib state (by calling zhc_update) before calling zhc_next_command");

    // NOTE(dgl): If the current command does not exist, we return the first from the list.
    // If it exists (e.g. from a earlier iteration) we return the next one.
    bool32 result = false;
    if (*cmd)
    {
        *cmd = (Zhc_Command *)(((uint8 *) *cmd) + (*cmd)->size);
    }
    else
    {
        *cmd = (Zhc_Command *)state->command_list;
    }

    if(((uint8 *) *cmd) < ((uint8 *)state->command_list + state->command_list_offset))
    {
        result = true;
    }
    return(result);
}
