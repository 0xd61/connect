enum Net_Msg_Header_Type
{
    Net_Msg_Header_Noop,
    Net_Msg_Header_Hash_Req,
    Net_Msg_Header_Hash_Res,
    Net_Msg_Header_Data_Req,
    Net_Msg_Header_Data_Res
};

struct Net_Msg_Header
{
    uint32 version; /* 16 bit major, 8 bit minor, 8 bit patch */
    Net_Msg_Header_Type type;
    uint32 size;
};

internal Zhc_Net_Socket
net_init_socket(DGL_Mem_Arena *arena, char *ip, uint16 port)
{
    Zhc_Net_Socket result = {};

    int32 index = 0;
    int32 number = 0;
    while(*ip)
    {
        if(*ip == '.')
        {
            assert(number <= 0xFF && number >= 0, "Invalid IP segment (cannot be bigger than 255)");
            result.address.ip[index++] = cast(uint8)number;
            number = 0;
            ++ip;
        }

        number *= 10;
        number += *ip - '0';

        ++ip;
    }

    assert(number <= 0xFF && number >= 0, "Invalid IP segment (cannot be bigger than 255)");
    assert(index == 3, "Invalid ip address");
    result.address.ip[index] = cast(uint8)number;
    result.address.port = port;

    platform.setup_socket(arena, &result);

    return(result);
}

// TODO(dgl): serialization
internal void
net_send_header(Zhc_Net_Socket *socket, Zhc_Net_IP *destination, Net_Msg_Header_Type type, usize size = 0)
{
    Net_Msg_Header header = {};
    header.type = type;
    header.version = parse_version(ZHC_VERSION);
    header.size = dgl_safe_size_to_uint32(size);

    platform.send_data(socket, destination, &header, sizeof(header));
}

internal void
net_send_hash_request(Zhc_Net_Socket *socket, Zhc_Net_IP *destination)
{
    net_send_header(socket, destination, Net_Msg_Header_Hash_Req);
}

internal void
net_send_hash_response(Zhc_Net_Socket *socket, Zhc_Net_IP *destination, uint32 hash)
{
    net_send_header(socket, destination, Net_Msg_Header_Hash_Res, sizeof(hash));
    platform.send_data(socket, destination, &hash, sizeof(hash));
}

internal void
net_send_data_request(Zhc_Net_Socket *socket, Zhc_Net_IP *destination)
{
    net_send_header(socket, destination, Net_Msg_Header_Data_Req);
}

internal void
net_send_data_response(Zhc_Net_Socket *socket, Zhc_Net_IP *destination, uint8 *buffer, usize buffer_size)
{
    net_send_header(socket, destination, Net_Msg_Header_Data_Res, buffer_size);
    platform.send_data(socket, destination, buffer, buffer_size);
}
