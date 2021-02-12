#include "zhc_platform.h"

struct Command_List
{
    usize offset;
    usize size;
    uint8 *memory;
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
    assert(sizeof(Lib_State) < memory->update_storage_size, "Not enough memory allocated");

    Lib_State *state = cast(Lib_State *)memory->update_storage;
    if(!state->is_initialized)
    {
        LOG_DEBUG("Lib_State size: %lld, Available memory: %lld", sizeof(*state), memory->update_storage_size);
        dgl_mem_arena_init(&state->permanent_arena, (uint8 *)memory->update_storage + sizeof(*state), ((DGL_Mem_Index)memory->update_storage_size - sizeof(*state))/2);
        dgl_mem_arena_init(&state->transient_arena, state->permanent_arena.base + state->permanent_arena.size, (DGL_Mem_Index)memory->update_storage_size - state->permanent_arena.size);

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

    V4 button = rect(0, 0, 50, 50);
    V2 menu_pos = v2(input->window.w - 3*(button.w + 20), 20);
    push_rect(&state->commands, rect(0, 0, input->window.w, input->window.h), color(1.0f,1.0f,1.0f,1.0f));
    push_rect(&state->commands, rect(menu_pos.x, menu_pos.y, button.w, button.h), color(1.0f,.8f,.3f,1.0f));
    push_rect(&state->commands, rect(menu_pos.x + button.w + 20, menu_pos.y, button.w, button.h), color(1.0f,.8f,.3f,1.0f));
    push_rect(&state->commands, rect(menu_pos.x + 2*(button.w + 20), menu_pos.y, button.w, button.h), color(1.0f,.8f,.3f,1.0f));
    LOG_DEBUG("Window %dx%d", input->window.w, input->window.h);
    push_text(&state->commands, rect(20, menu_pos.y + button.h + 10, input->window.w - 2*20, input->window.h - 20), color(0.0f, 0.0f, 0.0f, 1.0f), "Das ist ein Test und ein ganz langer langer text. Und hier ist ein newline\nI c h h o f f e e e e e e, dieser Text ist nicht zu lange.");
}

// NOTE(dgl): call this function only after zhc_update!!
bool32
zhc_next_command(Zhc_Memory *memory, Zhc_Command **cmd)
{
    Lib_State *state = cast(Lib_State *)memory->update_storage;
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
