#include "zhc_lib.h"
#include "zhc_asset.cpp"
#include "zhc_renderer.cpp"

#define DGL_IMPLEMENTATION
#include "dgl.h"

#include "dgl_test_helpers.h"

int
main(int argc, char **argv)
{

    uint8 data[megabytes(1)] = {};

    Render_Command_Buffer buffer = {};
    render_command_buffer_init(&buffer, data, kilobytes(256));

    DGL_BEGIN_TEST("Allocating a new render command based on type");
    {
        Render_Command_Rect *cmd = render_command_alloc(&buffer, Render_Command_Type_Rect_Filled, Render_Command_Rect);
    }
    DGL_END_TEST();

    if(dgl_test_result()) { return(0); }
    else { return(1); }
}
