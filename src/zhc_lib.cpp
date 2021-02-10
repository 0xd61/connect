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

struct Command_List
{
    usize offset;
    usize size;
    uint8 *memory;
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

    Command_List commands;

    bool32 is_initialized;
};

internal Zhc_Command *
push_command(Command_List *list, Zhc_Command_Type type, usize size)
{
    assert(list->memory, "Command list must be initialized");
    Zhc_Command *result = 0;

    // TODO(dgl): should we align the size?
    assert((list->offset + size) < list->size, "Command list overflow");

    result = (Zhc_Command *)(list->memory + list->offset);
    list->offset += size;
    result->type = type;
    result->size = dgl_safe_size_to_int32(size);

    return(result);
}

internal void
push_rect(Command_List *list, V4 rect, V4 color)
{
    Zhc_Command *cmd = push_command(list, Command_Type_Rect, sizeof(Zhc_Command));

    cmd->rect_cmd.rect = rect;
    cmd->rect_cmd.color = color;
}

internal void
push_text(Command_List *list, V4 rect, V4 color, char *text)
{
    usize size = cast(usize)string_length(text) + 1;
    Zhc_Command *cmd = push_command(list, Command_Type_Text, sizeof(Zhc_Command) + size);

    dgl_memcpy(cmd->text_cmd.text, text, size);
    cmd->text_cmd.rect = rect;
    cmd->text_cmd.color = color;
}

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
    // We free at the beginning of a new frame to have the memory
    // still available during the rendering.
    dgl_mem_arena_free_all(&state->transient_arena);

    state->commands.offset = 0;
    state->commands.size = kilobytes(256);

    state->commands.memory =
        dgl_mem_arena_push_array(&state->transient_arena, uint8, state->commands.size);

    push_rect(&state->commands, rect(0, 0, input->window.w, input->window.h), color(1,1,1,1));
    push_text(&state->commands, rect(0, 0, input->window.w, input->window.h), color(1,1,1,1), "Das ist ein Beispiel text");
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
        *cmd = (Zhc_Command *)state->commands.memory;
    }

    if(((uint8 *) *cmd) < ((uint8 *)state->commands.memory + state->commands.offset))
    {
        result = true;
    }
    return(result);
}
