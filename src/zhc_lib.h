#ifndef ZHC_LIB
#define ZHC_LIB

#include "zhc_platform.h"
#include "zhc_utils.h"
#include "zhc_asset.h"
#include "zhc_ui.h"

#define ZHC_VERSION "0.1.0"
#if ZHC_INTERNAL
#define ZHC_SERVER_IP "192.168.101.124"
#define ZHC_SERVER_PORT 1337
#else
#define ZHC_SERVER_IP "192.168.13.37"
#define ZHC_SERVER_PORT 1337
#endif

// TODO(dgl): check file size on read file or filegroup
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

    Zhc_Net_Socket net_socket;

    Zhc_Input old_input;
    bool32 force_render;

    bool32 is_initialized;
};

#endif // ZHC_LIB
