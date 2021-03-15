#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <sys/mman.h> /* mmap */

#include "zhc_platform.h"
#include <dirent.h> /* opendir, readdir */
#include <errno.h>
#include "sdl2_api.cpp"

#define DGL_IMPLEMENTATION
#include "dgl.h"

#include "dgl_test_helpers.h"
#include <unistd.h>

#define TEST_PORT 9876
#define TEST_HOST 16777343 // localhost

internal void
server_socket_setup(DGL_Mem_Arena *arena, Zhc_Net_Socket *server)
{
    server->address.host = 0;
    server->address.port = TEST_PORT;
    sdl_net_server_setup_socket_set(arena, server);
}

internal void
client_socket_setup(DGL_Mem_Arena *arena, Zhc_Net_Socket *client)
{
    client->address.host = TEST_HOST;
    client->address.port = TEST_PORT;
    sdl_net_client_setup_socket(arena, client);
}

int
main(int argc, char **argv)
{
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        LOG("Unable to initialize SDL: %s", SDL_GetError());
        return(1);
    }
    if(SDLNet_Init() != 0)
    {
        LOG("Unable to initialize SDLNet: %s", SDLNet_GetError());
        return(1);
    }

    usize memory_size = kilobytes(32);
    uint8 *memory_block = dgl_cast(uint8 *)mmap(0, memory_size,
                              PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

    DGL_Mem_Arena arena = {};
    dgl_mem_arena_init(&arena, memory_block, memory_size);

    DGL_BEGIN_TEST("Server listens for client connection");
    {
        Zhc_Net_Socket server = {};
        Zhc_Net_Socket client = {};
        server_socket_setup(&arena, &server);
        client_socket_setup(&arena, &client);

        SDL_Server_Socket *server_platform = dgl_cast(SDL_Server_Socket *)server.platform;
        SDL_Client_Socket *client_platform = dgl_cast(SDL_Client_Socket *)client.platform;

        DGL_EXPECT_bool32(server.no_error, ==, true);
        DGL_EXPECT_bool32(client.no_error, ==, true);
        DGL_EXPECT_ptr(server_platform->clients[0].socket, ==, 0);

        // NOTE(dgl): listen for client connection request
        // the peer address is only checked, if we received client message.
        sdl_net_server_receive_data(&server, 0, 0, 0);
        DGL_EXPECT_ptr(server_platform->clients[0].socket, >, 0);
        DGL_EXPECT_ptr(server_platform->clients[1].socket, ==, 0);

        SDLNet_TCP_Close(client_platform->socket);
        SDLNet_TCP_Close(server_platform->socket);
        dgl_mem_arena_free_all(&arena);
    }
    DGL_END_TEST();

    DGL_BEGIN_TEST("Server removes client on connection close");
    {
        Zhc_Net_Socket server = {};
        Zhc_Net_Socket client = {};
        server_socket_setup(&arena, &server);
        client_socket_setup(&arena, &client);
        sdl_net_server_receive_data(&server, 0, 0, 0);

        SDL_Server_Socket *server_platform = dgl_cast(SDL_Server_Socket *)server.platform;
        SDL_Client_Socket *client_platform = dgl_cast(SDL_Client_Socket *)client.platform;
        DGL_EXPECT_ptr(server_platform->clients[0].socket, >, 0);

        IPaddress client_address = *SDLNet_TCP_GetPeerAddress(server_platform->clients[0].socket);
        SDLNet_TCP_Close(client_platform->socket);
        Zhc_Net_IP address = {};
        sdl_net_server_receive_data(&server, &address, 0, 0);

        DGL_EXPECT_uint32(address.host, ==, client_address.host);
        DGL_EXPECT_uint16(address.port, ==, SDL_Swap16(client_address.port));
        DGL_EXPECT_ptr(server_platform->clients[0].socket, ==, 0);
        DGL_EXPECT_uint32(server_platform->clients[0].peer.host, ==, 0);
        DGL_EXPECT_uint16(server_platform->clients[0].peer.port, ==, 0);

        SDLNet_TCP_Close(server_platform->socket);
        dgl_mem_arena_free_all(&arena);
    }
    DGL_END_TEST();

    DGL_BEGIN_TEST("Server receive with address, returns data from the first client");
    {
        Zhc_Net_Socket server = {};
        Zhc_Net_Socket client1 = {};
        Zhc_Net_Socket client2 = {};
        server_socket_setup(&arena, &server);
        client_socket_setup(&arena, &client1);
        client_socket_setup(&arena, &client2);
        sdl_net_server_receive_data(&server, 0, 0, 0);
        sdl_net_server_receive_data(&server, 0, 0, 0);

        SDL_Server_Socket *server_platform = dgl_cast(SDL_Server_Socket *)server.platform;
        SDL_Client_Socket *client1_platform = dgl_cast(SDL_Client_Socket *)client1.platform;
        SDL_Client_Socket *client2_platform = dgl_cast(SDL_Client_Socket *)client2.platform;

        char *send_buf1 = "abc";
        char *send_buf2 = "def";
        Zhc_Net_IP server_address = { .host=TEST_HOST, .port=TEST_PORT };
        sdl_net_client_send_data(&client2, &server_address, dgl_cast(uint8 *)send_buf2, 4);
        sdl_net_client_send_data(&client1, &server_address, dgl_cast(uint8 *)send_buf1, 4);

        Zhc_Net_IP client_address = {};
        char recv_buf[4] = {};
        DGL_EXPECT_bool32(sdl_net_server_receive_data(&server, &client_address, recv_buf, 4), ==, true);
        DGL_EXPECT_int32(strcmp(recv_buf, send_buf1), ==, 0);
        DGL_EXPECT_uint32(server_platform->clients[0].peer.host, ==, client_address.host);
        DGL_EXPECT_uint16(server_platform->clients[0].peer.port, ==, SDL_Swap16(client_address.port));

        client_address = {};
        DGL_EXPECT_bool32(sdl_net_server_receive_data(&server, &client_address, recv_buf, 4), ==, false);
        DGL_EXPECT_int32(strcmp(recv_buf, send_buf2), ==, 0);
        DGL_EXPECT_uint32(server_platform->clients[1].peer.host, ==, client_address.host);
        DGL_EXPECT_uint16(server_platform->clients[1].peer.port, ==, SDL_Swap16(client_address.port));

        SDLNet_TCP_Close(client1_platform->socket);
        SDLNet_TCP_Close(client2_platform->socket);
        SDLNet_TCP_Close(server_platform->socket);
        dgl_mem_arena_free_all(&arena);
    }
    DGL_END_TEST();

    DGL_BEGIN_TEST("Server receive with address, returns only data from client");
    {
        Zhc_Net_Socket server = {};
        Zhc_Net_Socket client1 = {};
        Zhc_Net_Socket client2 = {};
        server_socket_setup(&arena, &server);
        client_socket_setup(&arena, &client1);
        client_socket_setup(&arena, &client2);
        sdl_net_server_receive_data(&server, 0, 0, 0);
        sdl_net_server_receive_data(&server, 0, 0, 0);

        SDL_Server_Socket *server_platform = dgl_cast(SDL_Server_Socket *)server.platform;
        SDL_Client_Socket *client1_platform = dgl_cast(SDL_Client_Socket *)client1.platform;
        SDL_Client_Socket *client2_platform = dgl_cast(SDL_Client_Socket *)client2.platform;

        char *send_buf1 = "abc";
        char *send_buf2 = "def";
        Zhc_Net_IP server_address = { .host=TEST_HOST, .port=TEST_PORT };
        sdl_net_client_send_data(&client2, &server_address, dgl_cast(uint8 *)send_buf2, 4);
        sdl_net_client_send_data(&client1, &server_address, dgl_cast(uint8 *)send_buf1, 4);

        Zhc_Net_IP client_address = { .host=server_platform->clients[1].peer.host,
                                      .port=SDL_Swap16(server_platform->clients[1].peer.port)};
        char recv_buf[4] = {};
        DGL_EXPECT_bool32(sdl_net_server_receive_data(&server, &client_address, recv_buf, 4), ==, true);
        DGL_EXPECT_int32(strcmp(recv_buf, send_buf2), ==, 0);
        DGL_EXPECT_uint32(server_platform->clients[1].peer.host, ==, client_address.host);
        DGL_EXPECT_uint16(server_platform->clients[1].peer.port, ==, SDL_Swap16(client_address.port));

        client_address = { .host=server_platform->clients[0].peer.host,
                           .port=SDL_Swap16(server_platform->clients[0].peer.port)};
        DGL_EXPECT_bool32(sdl_net_server_receive_data(&server, &client_address, recv_buf, 4), ==, false);
        DGL_EXPECT_int32(strcmp(recv_buf, send_buf1), ==, 0);
        DGL_EXPECT_uint32(server_platform->clients[0].peer.host, ==, client_address.host);
        DGL_EXPECT_uint16(server_platform->clients[0].peer.port, ==, SDL_Swap16(client_address.port));

        SDLNet_TCP_Close(client1_platform->socket);
        SDLNet_TCP_Close(client2_platform->socket);
        SDLNet_TCP_Close(server_platform->socket);
        dgl_mem_arena_free_all(&arena);
    }
    DGL_END_TEST();

    DGL_BEGIN_TEST("Client receives data");
    {
        Zhc_Net_Socket server = {};
        Zhc_Net_Socket client = {};
        server_socket_setup(&arena, &server);
        client_socket_setup(&arena, &client);
        sdl_net_server_receive_data(&server, 0, 0, 0);

        SDL_Server_Socket *server_platform = dgl_cast(SDL_Server_Socket *)server.platform;
        SDL_Client_Socket *client_platform = dgl_cast(SDL_Client_Socket *)client.platform;
        int8 msg = 123;
        Zhc_Net_IP client_address = { .host=server_platform->clients[0].peer.host,
                                      .port=SDL_Swap16(server_platform->clients[0].peer.port)};

        sdl_net_server_send_data(&server, &client_address, &msg, 1);

        Zhc_Net_IP address = {};
        int8 resv_msg = 0;
        sdl_net_client_receive_data(&client, &address, &resv_msg, 1);

        DGL_EXPECT_int8(msg, ==, resv_msg);

        SDLNet_TCP_Close(client_platform->socket);
        SDLNet_TCP_Close(server_platform->socket);
        dgl_mem_arena_free_all(&arena);
    }
    DGL_END_TEST();

    DGL_BEGIN_TEST("Loading a directory, loads all files ordered by filename into a filegroup");
    {
        Zhc_File_Group *group = get_directory_filenames(&arena, "../data/files");

        Zhc_File_Info *info = group->first_file_info;
        DGL_EXPECT_int32(strcmp(info->filename, "01-first_file.txt"), ==, 0);
        info = info->next;
        DGL_EXPECT_int32(strcmp(info->filename, "02-photograph.txt"), ==, 0);
        info = info->next;
        DGL_EXPECT_int32(strcmp(info->filename, "03-tabs.txt"), ==, 0);
        DGL_EXPECT_ptr(info->next, ==, 0);

        DGL_EXPECT_int32(strcmp(group->dirpath, "../data/files/"), ==, 0);

        dgl_mem_arena_free_all(&arena);
    }
    DGL_END_TEST();

    DGL_BEGIN_TEST("Directory group path always ends with a \\ or /");
    {
        Zhc_File_Group *group = get_directory_filenames(&arena, "../data/files");
        DGL_EXPECT_int32(strcmp(group->dirpath, "../data/files/"), ==, 0);
        Zhc_File_Group *group1 = get_directory_filenames(&arena, "../data/files/");
        DGL_EXPECT_int32(strcmp(group1->dirpath, "../data/files/"), ==, 0);
    }
    DGL_END_TEST();

    if(dgl_test_result()) { return(0); }
    else { return(1); }
}
