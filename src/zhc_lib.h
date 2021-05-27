#ifndef ZHC_LIB_H
#define ZHC_LIB_H

#include "zhc_platform.h"
#include "zhc_utils.h"
#include "zhc_asset.h"
#include "zhc_renderer.h"
#include "zhc_ui.h"
#include "zhc_net.h"

// TODO(dgl): use version stored in byte format.
/* 16 bit major, 8 bit minor, 8 bit patch */
#define ZHC_VERSION "0.2.2"
#define ZHC_SERVER_PORT 8888

// NOTE(dgl): This size is cannot be larger than the amount of bits
// that are available in an ACK package (MTU size - ACK header size)
// the current max is about 1.2 megabytes. Therefore we max the size at
// 1 megabyte. If larger files need to be sent, we need another ACK
// strategy. Currently each bit determines a chunk slice.
#define ZHC_MAX_FILESIZE megabytes(1)

#define ZHC_ASSET_MEMORY_SIZE megabytes(32)
#define ZHC_IO_MEMORY_SIZE megabytes(8)

global Zhc_Platform_Api platform;

struct File
{
    Zhc_File_Info *info;
    uint32 hash;
    uint8 *data;
};

struct Lib_State
{
    DGL_Mem_Arena permanent_arena;
    DGL_Mem_Arena transient_arena; // NOTE(dgl): cleared on each frame

    Render_Command_Buffer cmd_buffer;
    Imui_Context *ui_ctx;

    // TODO(dgl): replace this with assets?
    DGL_Mem_Arena io_arena; // NOTE(dgl): cleared on each update timeout
    real32 io_update_timeout;
    File active_file;
    Zhc_File_Group *files;
    int32 desired_file_id;

    Net_Context *net_ctx;

    Zhc_Input old_input;
    bool32 force_render;

    bool32 is_initialized;
};

#endif // ZHC_LIB_H
