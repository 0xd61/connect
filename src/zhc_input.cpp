// NOTE(dgl): max 32 supported
enum Zhc_Mouse_Button
{
    Zhc_Mouse_Button_Left = (1 << 0),
    Zhc_Mouse_Button_Middle = (1 << 1),
    Zhc_Mouse_Button_Right = (1 << 2)
};

// NOTE(dgl): max 32 supported
enum Zhc_Keyboard_Button
{
    Zhc_Keyboard_Button_Shift = (1 << 0),
    Zhc_Keyboard_Button_Ctrl = (1 << 1),
    Zhc_Keyboard_Button_Alt = (1 << 2),
    Zhc_Keyboard_Button_Del = (1 << 3),
    Zhc_Keyboard_Button_Backspace = (1 << 4),
    Zhc_Keyboard_Button_Enter = (1 << 5)
};

struct Zhc_Input
{
    V2 pos;
    V2 last_pos;

    V2 scroll_delta;

    // NOTE(dgl): only modifier key. We get the text from SDL2.
    int32 key_down;
    int32 mouse_down;

    // NOTE(dgl): text from text input event
    char text[32];
};

void
zhc_keybutton(Zhc_Input *input, Zhc_Keyboard_Button key, bool32 down)
{
    if(down) { input->key_down |= key; }
    else { input->key_down &= ~key; }
}

void
zhc_mousebutton(Zhc_Input *input, Zhc_Mouse_Button button, bool32 down)
{
    if(down) { input->mouse_down |= button; }
    else { input->mouse_down &= ~button; }
}

void
zhc_mousemove(Zhc_Input *input, V2 pos)
{
    input->pos = pos;
}

void
zhc_mousescroll(Zhc_Input *input, V2 delta)
{
    input->scroll_delta.x += delta.x;
    input->scroll_delta.y += delta.y;
}

internal int32
string_length(char *s)
{
    int32 result = 0;
    while(*s++) { ++result; }

    return(result);
}

void
zhc_text(Zhc_Input *input, char *text)
{
    int32 len = string_length(input->text);
    int32 size = string_length(text);

    assert(len + size < array_count(input->text), "Text input overflow");
    char *dest = input->text + len;
    dgl_memcpy(dest, text, cast(usize)size);
    dest[size] = '\0';
}

void
zhc_reset(Zhc_Input *input)
{
    input->key_down = 0;
    input->text[0] = '\0';
    input->mouse_down = 0;
    input->scroll_delta = v2(0, 0);
    input->last_pos = input->pos;
}
