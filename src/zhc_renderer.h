#ifndef ZHC_RENDERER_H
#define ZHC_RENDERER_H


// TODO(dgl): we currently have three issues with cached rendering.
// 1. On some resolutions nothing is drawn
// 2. On some resolutions the screen is not updated (I guess this is solved)
// 3. On small resolutions we hit an assert in draw_rectangle/4 (Render buffer overflow)
#define CACHED_RENDERING       true
#define CACHED_RENDERING_DEBUG true


struct Render_Command_Buffer
{
    usize size;
    usize offset;
    uint8 *base;
    bool32 is_initialized;
};

enum Render_Command_Type
{
    Render_Command_Type_Noop,
    Render_Command_Type_Rect_Filled,
    Render_Command_Type_Image,
    Render_Command_Type_Font
};

struct Render_Command
{
    usize size;
    Render_Command_Type type;
    //Render_Command *next; /* we could calculate this. Should we align the data? */
};

struct Render_Command_Rect
{
    V4 rect;
    V4 color;
};

// NOTE(dgl): Maybe it would be easier to use the rect as render rect and pos just
// as the starting pos of the source image?
struct Render_Command_Image
{
    V2 pos;
    V4 rect;
    V4 color;
    Asset_ID image;
};

struct Render_Command_Font
{
    V2 pos;
    V4 color;
    uint32 codepoint;
    Asset_ID font;
    Asset_ID bitmap;
    int32 size;
};

struct Render_Glyph
{
    V4 coordinates;
    V2 offset;
};

struct Hash_Grid
{
    int32 cell_count_x;
    int32 cell_count_y;
    uint32 *cells;
    uint32 *prev_cells;

    // NOTE(dgl): Rectangles to be rendered
    int32 render_rect_count; // TODO(dgl): is this needed? It will never be exceeded... @cleanup
    V4 *render_rects;
};

struct Render_Context
{
    // TODO(dgl): need to refactor the asset loading.
    // Until then, we pass them during rendering.
    //Zhc_Assets *assets;
    Hash_Grid *grid;
    V4 clipping_rect;
};

#endif // ZHC_RENDERER_H
