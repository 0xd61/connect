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
    Screen_Size_SM = 720,
    Screen_Size_MD = 1024,
    Screen_Size_LG = 1280,
    Screen_Size_XL = 1536
};

struct Button_Theme
{
    int32 icon_size;
    V4 icon_color;
    V4 hover_color;
    V4 bg_color;
};

struct Theme
{
    // NOTE(dgl): sizes are the recommended sizes for
    // the screen size.
    int32 font_size;
    int32 icon_size;
    V4 primary_color;
    //V4 secondary_color;
    V4 bg_color;
    V2 menu_size;
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

struct Imui_Context
{
    V2 window;
    Screen_Size screen;
    bool32 is_dark;

    Element_ID active;
    Element_ID hot;
    // NOTE(dgl): the last hot element of the last frame
    // as a simple way to handle toverlapping elements.
    Element_ID top_most_hot;
    bool32 hot_updated;

    Zhc_Assets *assets;

    Icon_Set icon_sets[6]; /* 24, 32, 48, 64, 96, 128 */

    Font system_font;
    Font text_font;

    Zhc_Input *input;
    Zhc_Offscreen_Buffer *buffer;

    Stack(Element_ID) id_stack;
    Stack(Element_State) element_state_list;
};

internal Theme get_default_theme(Screen_Size screen);

#endif // ZHC_UI
