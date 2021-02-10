struct Zhc_Offscreen_Buffer
{
    int32 width;
    int32 height;
    int32 pitch;
    int32 bytes_per_pixel;
    void *memory;
};

struct Zhc_Renderer
{
    SDL_Window *window;
    Zhc_Offscreen_Buffer draw_buffer;
    V4 clip;
};

internal void
set_clip(Zhc_Renderer *renderer, V4 rect)
{
    renderer->clip.left = rect.x;
    renderer->clip.top = rect.y;
    renderer->clip.right = rect.x + rect.w;
    renderer->clip.bottom = rect.y + rect.h;
}

void
shz_update_buffer_size(Zhc_Renderer *renderer)
{
    SDL_Surface *surf = SDL_GetWindowSurface(renderer->window);
    renderer->draw_buffer.width = surf->w;
    renderer->draw_buffer.height = surf->h;
}

void
zhc_render_init(Zhc_Renderer *renderer, SDL_Window *window)
{
    renderer->window = window;
    SDL_Surface *surf = SDL_GetWindowSurface(renderer->window);
    renderer->draw_buffer.width = surf->w;
    renderer->draw_buffer.height = surf->h;
    renderer->draw_buffer.pitch = surf->pitch;
    renderer->draw_buffer.bytes_per_pixel = surf->format->BytesPerPixel;
    renderer->draw_buffer.memory = surf->pixels;
    set_clip(renderer, rect(0,0,surf->w, surf->h));
}

void
zhc_render_rect(Zhc_Renderer *renderer)
{

}

void
zhc_render_text(Zhc_Renderer *renderer)
{

}
