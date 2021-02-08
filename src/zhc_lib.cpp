
enum Zhc_Command_Type
{
    Command_Type_Rect,
    Command_Type_Text,
};

struct Zhc_Command_List
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
    DGL_Mem_Arena frame_arena;

    bool32 is_initialized;
};

void
zhc_update(Zhc_Memory *memory)
{
    assert(sizeof(Lib_State) < memory->storage_size, "Not enough memory allocated");

    Lib_State *state = cast(Lib_State *)memory->storage;
    if(!state->is_initialized)
    {

    }
}
