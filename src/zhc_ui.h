#ifndef ZHC_UI
#define ZHC_UI

// TODO(dgl): we have a bug in stbtt_BakeFontBitmap with font size 108px
#define MAX_FONT_SIZE 104

typedef uint32 Element_ID;

enum Screen_Size
{
    Screen_Uninitialized = 0,
    Screen_Size_XS = 640,
    Screen_Size_SM = 768,
    Screen_Size_MD = 1024,
    Screen_Size_LG = 1280,
    Screen_Size_XL = 1536
};

struct Theme
{
    Screen_Size type;
    int32 font_size;
    V2 menu_size;
    V4 fg_color;
    V4 bg_color;
};

struct Font
{
    uint8 *ttf_buffer;
    Zhc_Image bitmap;
    int32 size; /* size in pixels */
    real32 linegap;
    real32 height;
    stbtt_fontinfo stbfont;
    // NOTE(dgl): we only encode ASCII + Latin-1 (first 256 code points)
    // if we need more, use glyphsets with each 256 characters to reduce
    // the amount of memory needed.
    stbtt_bakedchar glyphs[256];
};

struct Element_State
{
    Element_ID id;
    V2 content;
    int32 scroll_pos; /* only vertical scrolling */
};

#define Stack(Type) struct{usize count; usize offset; Type *memory;}

struct Imui_Context
{
    V2 window;
    Element_ID active;
    Element_ID hot;
    // NOTE(dgl): the last hot element of the last frame
    // as a simple way to handle toverlapping elements.
    Element_ID top_most_hot;
    bool32 hot_updated;

    Theme theme;

    // NOTE(dgl): to be able to reset the font
    // and have more safety, if we overflow the arena.
    DGL_Mem_Arena font_arena;
    Font *system_font;
    Font *text_font;

    Zhc_Input *input;
    Zhc_Offscreen_Buffer *buffer;

    Stack(Element_ID) id_stack;
    Stack(Element_State) element_state_list;

    int32 desired_text_font_size; /* in pixels */
};

#endif // ZHC_UI
