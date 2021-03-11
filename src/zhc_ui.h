#ifndef ZHC_UI
#define ZHC_UI

#include "lib/stb_truetype.h"

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
    int32 icon_size;
    V2 menu_size;
    V4 fg_color;
    V4 bg_color;
};

enum Icon_Type
{
    Icon_Type_Increase_Font,
    Icon_Type_Decrease_Font,
    Icon_Type_Dark,
    Icon_Type_Light,
    Icon_Type_Next,
    Icon_Type_Previous,
    Icon_Type_Count
};

struct Icon
{
    Asset_ID bitmap;
    Icon_Type type;
    V4 box;
};

struct Icon_Set
{
    int32 size;
    Icon icons[Icon_Type_Count];
};

struct Font
{
    Asset_ID font_asset;
    Asset_ID bitmap;

    int32 size; /* size in pixels */
    real32 linegap;
    real32 height;
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
    Zhc_Assets *assets;

    Icon_Set icon_sets[5]; /* 16, 25, 32, 64, 128 */

    Font system_font;
    Font text_font;
    int32 desired_text_font_size; /* in pixels */

    Zhc_Input *input;
    Zhc_Offscreen_Buffer *buffer;

    Stack(Element_ID) id_stack;
    Stack(Element_State) element_state_list;
};

#endif // ZHC_UI
