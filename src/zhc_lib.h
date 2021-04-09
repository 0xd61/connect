#ifndef ZHC_LIB_H
#define ZHC_LIB_H

#include "zhc_platform.h"
#include "zhc_utils.h"
#include "zhc_asset.h"
#include "zhc_ui.h"
#include "zhc_net.h"

#define ZHC_VERSION "0.1.0"
#if ZHC_INTERNAL
#define ZHC_SERVER_IP "192.168.101.124"
#define ZHC_SERVER_PORT 1337
#else
#define ZHC_SERVER_IP "192.168.13.37"
#define ZHC_SERVER_PORT 1337
#endif

// NOTE(dgl): This size is cannot be larger than the amount of bits
// that are available in an ACK package (MTU size - ACK header size)
// the current max is about 1.2 megabytes. Therefore we max the size at
// 1 megabyte. If larger files need to be sent, we need another ACK
// strategy. Currently each bit determines a chunk slice.
#define ZHC_MAX_FILESIZE megabytes(1)

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
